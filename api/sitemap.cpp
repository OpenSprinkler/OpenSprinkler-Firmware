#include "sitemap.h"

#include "handler_context.h"
#include "handlers.h"
#include "sitemap_relay.h"

#include "storage/files.h"
#include "storage/maintenance.h"

#include <cstdlib>
#include <cstring>
#include <new>

#if !defined(SITEMAP_DEVICE_IMAGE)
#include <openssl/evp.h>
#endif
#if defined(ESP8266)
#include <flash_hal.h>
#include "services/firmware_update.h"
#endif

/*
 * Site map storage for the apps: station pins on a background image.
 *
 * smap.dat layout (little endian):
 *   uint32 revision, uint32 image size, uint16 pin count,
 *   then per pin: uint8 station index, uint16 x, uint16 y (0 to SITEMAP_COORD_MAX across the image)
 * The JPEG image is stored separately in smapimg.dat and uploaded in base64 chunks through smapimg.tmp;
 * base64 keeps the upload intact when it is relayed as text through the OpenThings cloud.
 * Head runs and drip lines are stored as app-defined ASCII text in smapdrw.dat, uploaded in plain text chunks
 * through smapdrw.tmp; the firmware only checks that it is printable text.
 * smapfp.dat holds the image fingerprint, the first 16 bytes of its SHA-256 as 32 lowercase hex digits, so apps can
 * tell whether the copy they have is current.
 *
 * With SITEMAP_DEVICE_IMAGE (OpenSprinkler v3/v4) there is no room for the image: the apps keep it, report its size
 * and fingerprint with /cmi, and pass it between themselves through the relay (sitemap_relay.h), whose current batch
 * is kept in smapxfr.tmp.
 */

namespace {

constexpr uint16_t SITEMAP_COORD_MAX = 10000;
constexpr uint32_t SITEMAP_HEADER_SIZE = 10;
constexpr uint32_t SITEMAP_PIN_SIZE = 5;

struct SiteMapHeader {
	uint32_t revision;
	uint32_t image_size;
	uint16_t pin_count;
};

int64_t cached_revision = -1;

void put_u16(uint8_t* bytes, uint16_t value) {
	bytes[0] = value & 0xFF;
	bytes[1] = value >> 8;
}

void put_u32(uint8_t* bytes, uint32_t value) {
	for (unsigned char index = 0; index < 4; index++) bytes[index] = (value >> (index * 8)) & 0xFF;
}

uint16_t get_u16(const uint8_t* bytes) {
	return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

uint32_t get_u32(const uint8_t* bytes) {
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

// A missing or unreadable file is an empty map with revision 0.
SiteMapHeader read_header() {
	SiteMapHeader header = {0, 0, 0};
	uint8_t bytes[SITEMAP_HEADER_SIZE];
	if (file_exists(SITEMAP_FILENAME) && file_read_block(SITEMAP_FILENAME, bytes, 0, SITEMAP_HEADER_SIZE)) {
		header.revision = get_u32(bytes);
		header.image_size = get_u32(bytes + 4);
		header.pin_count = get_u16(bytes + 8);
	}
	return header;
}

void encode_header(const SiteMapHeader& header, uint8_t* bytes) {
	put_u32(bytes, header.revision);
	put_u32(bytes + 4, header.image_size);
	put_u16(bytes + 8, header.pin_count);
}

// Rewrites the whole file: the header followed by count pins. The new map is written to a temp file and renamed over
// the old one, so a power cut while writing leaves the previous map, and the image size it records, intact.
bool write_map(const SiteMapHeader& header, const uint8_t* pins) {
	uint8_t bytes[SITEMAP_HEADER_SIZE];
	encode_header(header, bytes);
	os_file_type file = file_open(SITEMAP_TEMP_FILENAME, FileOpenMode::WriteTruncate);
	if (!file) return false;
	uint32_t pins_length = (uint32_t)header.pin_count * SITEMAP_PIN_SIZE;
	bool ok = file_write(file, bytes, SITEMAP_HEADER_SIZE) == (int)SITEMAP_HEADER_SIZE &&
		(pins_length == 0 || file_write(file, pins, pins_length) == (int)pins_length);
	file_close(file);
	if (!ok || !rename_file(SITEMAP_TEMP_FILENAME, SITEMAP_FILENAME)) {
		remove_file(SITEMAP_TEMP_FILENAME);
		return false;
	}
	cached_revision = header.revision;
	return true;
}

// Updates the header while keeping the stored pins.
bool write_header(const SiteMapHeader& header) {
	if (!file_exists(SITEMAP_FILENAME)) {
		SiteMapHeader empty = header;
		empty.pin_count = 0;
		return write_map(empty, nullptr);
	}
	uint8_t bytes[SITEMAP_HEADER_SIZE];
	encode_header(header, bytes);
	if (!file_write_block(SITEMAP_FILENAME, bytes, 0, SITEMAP_HEADER_SIZE)) return false;
	cached_revision = header.revision;
	return true;
}

uint32_t current_file_size(const char* filename) {
	if (!file_exists(filename)) return 0;
	os_file_type file = file_open(filename, FileOpenMode::Read);
	if (!file) return 0;
	uint32_t size = file_size(file);
	file_close(file);
	return size;
}

bool parse_uint(const char* text, uint32_t max_value, uint32_t& value) {
	if (!text || !text[0]) return false;
	char* end = nullptr;
	unsigned long parsed = strtoul(text, &end, 10);
	if (*end || parsed > max_value) return false;
	value = (uint32_t)parsed;
	return true;
}

int base64_value(char c) {
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

// Decodes base64 in place; the output is always shorter than the input.
bool decode_base64(char* data, size_t length, uint32_t& decoded) {
	if (length % 4) return false;
	decoded = 0;
	for (size_t position = 0; position < length; position += 4) {
		int values[4];
		unsigned char padding = 0;
		for (unsigned char index = 0; index < 4; index++) {
			char c = data[position + index];
			bool last_group = position + 4 == length;
			if (c == '=' && last_group && index >= 2) {
				values[index] = 0;
				padding++;
				continue;
			}
			values[index] = padding ? -1 : base64_value(c);
			if (values[index] < 0) return false;
		}
		uint32_t bits = ((uint32_t)values[0] << 18) | ((uint32_t)values[1] << 12) | ((uint32_t)values[2] << 6) | (uint32_t)values[3];
		data[decoded++] = (char)((bits >> 16) & 0xFF);
		if (padding < 2) data[decoded++] = (char)((bits >> 8) & 0xFF);
		if (padding < 1) data[decoded++] = (char)(bits & 0xFF);
	}
	return true;
}

bool is_drawing_text(const char* data, uint32_t length) {
	for (uint32_t index = 0; index < length; index++) {
		char c = data[index];
		if (c != '\n' && (c < 0x20 || c > 0x7E)) return false;
	}
	return true;
}

constexpr uint32_t SITEMAP_FINGERPRINT_LENGTH = 32;

bool is_fingerprint(const char* text) {
	if (strlen(text) != SITEMAP_FINGERPRINT_LENGTH) return false;
	for (const char* c = text; *c; c++) {
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f'))) return false;
	}
	return true;
}

// Reads the stored fingerprint into fingerprint (SITEMAP_FINGERPRINT_LENGTH + 1 bytes); empty when there is none.
void read_fingerprint(char* fingerprint) {
	fingerprint[0] = 0;
	if (!file_exists(SITEMAP_FINGERPRINT_FILENAME) ||
		!file_read_block(SITEMAP_FINGERPRINT_FILENAME, fingerprint, 0, SITEMAP_FINGERPRINT_LENGTH)) {
		return;
	}
	fingerprint[SITEMAP_FINGERPRINT_LENGTH] = 0;
	if (!is_fingerprint(fingerprint)) fingerprint[0] = 0;
}

bool write_fingerprint(const char* fingerprint) {
	remove_file(SITEMAP_FINGERPRINT_FILENAME);
	return file_write_block(SITEMAP_FINGERPRINT_FILENAME, fingerprint, 0, SITEMAP_FINGERPRINT_LENGTH);
}

#if !defined(SITEMAP_DEVICE_IMAGE)
// Fingerprints the stored image and saves the result; fingerprint is left empty if the image can't be read.
void fingerprint_image(char* fingerprint) {
	fingerprint[0] = 0;
	remove_file(SITEMAP_FINGERPRINT_FILENAME);
	os_file_type file = file_open(SITEMAP_IMAGE_FILENAME, FileOpenMode::Read);
	if (!file) return;
	EVP_MD_CTX* context = EVP_MD_CTX_new();
	bool ok = context && EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
	int count = 0;
	while (ok && (count = file_read(file, tmp_buffer, TMP_BUFFER_SIZE)) > 0) {
		ok = EVP_DigestUpdate(context, tmp_buffer, count);
	}
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int length = 0;
	ok = ok && count == 0 && EVP_DigestFinal_ex(context, digest, &length) && length >= SITEMAP_FINGERPRINT_LENGTH / 2;
	EVP_MD_CTX_free(context);
	file_close(file);
	if (!ok) return;

	static const char hex[] = "0123456789abcdef";
	for (uint32_t index = 0; index < SITEMAP_FINGERPRINT_LENGTH / 2; index++) {
		fingerprint[index * 2] = hex[digest[index] >> 4];
		fingerprint[index * 2 + 1] = hex[digest[index] & 0x0F];
	}
	fingerprint[SITEMAP_FINGERPRINT_LENGTH] = 0;
	if (!write_fingerprint(fingerprint)) fingerprint[0] = 0;
}
#endif

// Whether filename holds data at position.
bool file_matches(const char* filename, uint32_t position, const char* data, uint32_t length) {
	os_file_type file = file_open(filename, FileOpenMode::Read);
	if (!file) return false;
	bool same = file_seek(file, position);
	char stored[64];
	while (same && length > 0) {
		uint32_t chunk = length < sizeof(stored) ? length : sizeof(stored);
		same = file_read(file, stored, chunk) == (int)chunk && memcmp(stored, data, chunk) == 0;
		data += chunk;
		length -= chunk;
	}
	file_close(file);
	return same;
}

// Result of write_upload_chunk for the last chunk resent after the upload completed; nothing was written.
constexpr unsigned char UPLOAD_RESENT_LAST = 0xFE;

// Writes one chunk of a chunked upload to temp_file, then moves it to file once total bytes have arrived.
// A chunk is resent when its reply was lost, so the chunk written last is accepted again without writing it,
// and so is the last chunk once the upload has completed.
// Returns HTML_OK when more chunks are expected, HTML_SUCCESS when the upload completed, UPLOAD_RESENT_LAST,
// or an error code.
unsigned char write_upload_chunk(const char* temp_file, const char* file, const char* data, uint32_t length,
	uint32_t offset, uint32_t total) {
	if (!total || offset + length > total) return HTML_DATA_OUTOFBOUND;

	if (offset == 0) {
		remove_file(temp_file);
#if defined(ARDUINO)
		EmbeddedStorageUsage usage = get_embedded_storage_usage();
		if (usage.valid && usage.free_bytes < total + embedded_storage_reserve_bytes(usage)) {
			return HTML_DATA_OUTOFBOUND;
		}
#endif
	} else if (!file_exists(temp_file)) {
		bool resent = offset + length == total && current_file_size(file) == total &&
			file_matches(file, offset, data, length);
		return resent ? UPLOAD_RESENT_LAST : HTML_DATA_OUTOFBOUND;
	} else {
		uint32_t size = current_file_size(temp_file);
		if (offset + length == size && file_matches(temp_file, offset, data, length)) return HTML_OK;
		if (size != offset) return HTML_DATA_OUTOFBOUND;
	}

	if (!file_write_block(temp_file, data, offset, length)) {
		remove_file(temp_file);
		return HTML_UPLOAD_FAILED;
	}
	if (offset + length < total) return HTML_OK;
	if (!rename_file(temp_file, file)) {
		remove_file(temp_file);
		return HTML_INTERNAL_ERROR;
	}
	return HTML_SUCCESS;
}

bool bump_revision() {
	SiteMapHeader header = read_header();
	header.revision++;
	return write_header(header);
}

// The data a request carries: its body, or when it has none, the URL parameter named key. OpenThings Cloud doesn't
// pass on POST requests that have a body, so apps connected through it send the data in the URL instead.
static char* request_data(const OTF::Request& req, const char* key, size_t& length) {
	char* data = req.getBody();
	length = data ? req.getBodyLength() : 0;
	if (!length) {
		data = req.getQueryParameter(key);
		length = data ? strlen(data) : 0;
	}
	return length ? data : nullptr;
}

#if defined(ARDUINO)
using flash_text = const __FlashStringHelper*;
#else
using flash_text = const char*;
#endif

// Writes the headers of a response whose body of size bytes follows
void begin_file_response(OTF::Response& res, uint32_t size, flash_text content_type) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), content_type);
	res.writeHeader(F("Content-Length"), (int)size);
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	res.writeHeader(F("Cache-Control"), F("max-age=0, no-cache, no-store, must-revalidate"));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData("", 0);
}

void begin_binary_response(OTF::Response& res, uint32_t size) {
	begin_file_response(res, size, F("application/octet-stream"));
}

// Streams a stored file as the response body. Returns false before responding when the file is missing, can't be
// opened, is empty or isn't size bytes long (any size when size is 0), so the caller can send a result code instead.
bool send_file(OTF::Response& res, const char* filename, uint32_t size, flash_text content_type) {
	if (!file_exists(filename)) return false;
	os_file_type file = file_open(filename, FileOpenMode::Read);
	if (!file) return false;
	uint32_t stored = file_size(file);
	if (!stored || (size && stored != size)) {
		file_close(file);
		return false;
	}
	size = stored;
	begin_file_response(res, size, content_type);

	uint32_t remaining = size;
	while (remaining > 0 && res.isValid()) {
		uint32_t chunk = remaining < TMP_BUFFER_SIZE ? remaining : TMP_BUFFER_SIZE;
		if (file_read(file, tmp_buffer, chunk) != (int)chunk) break;
		res.writeBodyData(tmp_buffer, chunk);
		remaining -= chunk;
	}
	file_close(file);
	return true;
}

// Parses "sid,x,y;sid,x,y;..." into packed pins. Returns an HTML_* result code.
unsigned char parse_pins(const char* source, size_t length, uint8_t* pins, uint16_t& count) {
	uint8_t seen[(MAX_NUM_STATIONS + 7) / 8] = {0};
	count = 0;
	size_t position = 0;
	while (position < length) {
		uint32_t values[3];
		for (unsigned char field = 0; field < 3; field++) {
			char number[8];
			size_t digits = 0;
			while (position < length && source[position] >= '0' && source[position] <= '9') {
				if (digits >= sizeof(number) - 1) return HTML_DATA_FORMATERROR;
				number[digits++] = source[position++];
			}
			number[digits] = 0;
			char expected = field < 2 ? ',' : ';';
			bool at_end = position >= length;
			if (!digits || (!at_end && source[position] != expected) || (at_end && field < 2)) {
				return HTML_DATA_FORMATERROR;
			}
			if (!at_end) position++;
			values[field] = strtoul(number, nullptr, 10);
		}

		uint32_t sid = values[0];
		if (sid >= os.nstations || values[1] > SITEMAP_COORD_MAX || values[2] > SITEMAP_COORD_MAX) {
			return HTML_DATA_OUTOFBOUND;
		}
		if (seen[sid >> 3] & (1 << (sid & 7))) return HTML_DATA_FORMATERROR;
		seen[sid >> 3] |= 1 << (sid & 7);

		uint8_t* pin = pins + (uint32_t)count * SITEMAP_PIN_SIZE;
		pin[0] = (uint8_t)sid;
		put_u16(pin + 1, (uint16_t)values[1]);
		put_u16(pin + 3, (uint16_t)values[2]);
		count++;
	}
	return HTML_SUCCESS;
}

#if defined(SITEMAP_DEVICE_IMAGE)
// Keeps the relay's current batch in a file, sized to leave the space set aside for logs
class RelayFileStorage : public SiteMapRelayStorage {
public:
	uint32_t available_bytes() override {
#if defined(ARDUINO)
		EmbeddedStorageUsage usage = get_embedded_storage_usage();
		if (!usage.valid) return 0;
		uint32_t keep = embedded_storage_reserve_bytes(usage) + 2 * usage.block_size;
		return usage.free_bytes > keep ? usage.free_bytes - keep : 0;
#else
		return SITEMAP_RELAY_MAX_WINDOW;
#endif
	}

	bool append(const uint8_t* data, uint32_t length) override {
		os_file_type file = file_open(SITEMAP_RELAY_FILENAME, FileOpenMode::Append);
		if (!file) return false;
		bool ok = file_write(file, data, length) == (int)length;
		file_close(file);
		return ok;
	}

	void clear() override {
		remove_file(SITEMAP_RELAY_FILENAME);
	}
};

#if defined(ESP8266)
// Firmware updates are staged in the free flash after the firmware, from the moment one is prepared until the
// controller restarts into it
bool update_owns_flash() {
	FirmwareUpdatePhase phase = firmware_update.phase();
	return phase == FirmwareUpdatePhase::Prepared || phase == FirmwareUpdatePhase::Uploading ||
		phase == FirmwareUpdatePhase::Verifying || phase == FirmwareUpdatePhase::Success;
}

// Keeps the relay's current batch in the free flash after the firmware, a raw sector at a time, which is several
// times faster than writing a file. Nothing is written there while a firmware update is under way.
class RelayFlashStorage : public SiteMapRelayStorage {
public:
	uint32_t available_bytes() override {
		uint32_t start = region_start();
		if (update_owns_flash() || FS_PHYS_ADDR <= start) return 0;
		return FS_PHYS_ADDR - start;
	}

	// Each sector is erased when the batch first reaches it. Writes must stay 4-byte aligned, so only the batch's
	// last piece may have an odd length.
	bool append(const uint8_t* data, uint32_t size) override {
		if (update_owns_flash() || (length & 3) || region_start() + length + size > FS_PHYS_ADDR) return false;
		while (size) {
			uint32_t address = region_start() + length;
			if (address % FLASH_SECTOR_SIZE == 0 && !ESP.flashEraseSector(address / FLASH_SECTOR_SIZE)) return false;
			uint32_t chunk = FLASH_SECTOR_SIZE - address % FLASH_SECTOR_SIZE;
			if (chunk > sizeof(staging)) chunk = sizeof(staging);
			if (chunk > size) chunk = size;
			uint32_t padded = (chunk + 3) & ~3UL;
			memcpy(staging, data, chunk);
			memset((uint8_t*)staging + chunk, 0xFF, padded - chunk);
			if (!ESP.flashWrite(address, staging, padded)) return false;
			length += chunk;
			data += chunk;
			size -= chunk;
		}
		return true;
	}

	void clear() override {
		length = 0;
	}

	// Streams the batch as the response body
	bool send(OTF::Response& res, uint32_t size) {
		if (size != length) return false;
		begin_binary_response(res, size);
		for (uint32_t sent = 0; sent < size && res.isValid();) {
			uint32_t chunk = size - sent < sizeof(staging) ? size - sent : sizeof(staging);
			if (!ESP.flashRead(region_start() + sent, staging, (chunk + 3) & ~3UL)) break;
			res.writeBodyData((const char*)staging, chunk);
			sent += chunk;
		}
		return true;
	}

private:
	uint32_t length = 0;
	uint32_t staging[64];

	// The first sector after the running firmware
	static uint32_t region_start() {
		static uint32_t start = (ESP.getSketchSize() + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
		return start;
	}
};

RelayFlashStorage relay_storage;
#else
RelayFileStorage relay_storage;
#endif
SiteMapRelay relay_state(relay_storage, SITEMAP_RELAY_REQUEST_BYTES, SITEMAP_RELAY_MAX_WINDOW,
	SITEMAP_RELAY_SENDER_LEASE_MS, SITEMAP_RELAY_RECEIVER_LEASE_MS);

// The relay, told of the stored image size
SiteMapRelay& relay() {
	relay_state.set_image_size(read_header().image_size);
#if defined(ESP8266)
	if (update_owns_flash()) relay_state.reset();
#endif
	return relay_state;
}

// Reads the id an app identifies itself with: cid, 1 to 8 hex digits. Returns an HTML_* result code.
unsigned char parse_client(const OTF::Request& req, uint32_t& client) {
	const char* text = req.getQueryParameter("cid");
	if (!text) return HTML_DATA_MISSING;
	size_t length = strlen(text);
	if (!length || length > 8) return HTML_DATA_FORMATERROR;
	client = 0;
	for (const char* c = text; *c; c++) {
		int digit = (*c >= '0' && *c <= '9') ? *c - '0' : (*c >= 'a' && *c <= 'f') ? *c - 'a' + 10 :
			(*c >= 'A' && *c <= 'F') ? *c - 'A' + 10 : -1;
		if (digit < 0) return HTML_DATA_FORMATERROR;
		client = (client << 4) | (uint32_t)digit;
	}
	return client ? HTML_SUCCESS : HTML_DATA_FORMATERROR;
}

// Reads the client id, and for requests that name the image (fp), checks it is still the stored one
unsigned char check_relay_request(const OTF::Request& req, uint32_t& client, bool names_image) {
	unsigned char result = parse_client(req, client);
	if (result != HTML_SUCCESS || !names_image) return result;
	const char* given = req.getQueryParameter("fp");
	if (!given) return HTML_DATA_MISSING;
	char stored[SITEMAP_FINGERPRINT_LENGTH + 1];
	read_fingerprint(stored);
	return stored[0] && strcmp(given, stored) == 0 ? HTML_SUCCESS : HTML_MISMATCH;
}
#endif

} // namespace

void sitemap_remove_temp_files() {
	remove_file(SITEMAP_TEMP_FILENAME);
	remove_file(SITEMAP_IMAGE_TEMP_FILENAME);
	remove_file(SITEMAP_DRAWINGS_TEMP_FILENAME);
	remove_file(SITEMAP_RELAY_FILENAME);
}

uint32_t sitemap_revision() {
	if (cached_revision < 0) cached_revision = read_header().revision;
	return (uint32_t)cached_revision;
}

/** Output the site map pins
 * Command: /jm?pw=xxx
 * Returns {"rev":N,"img":image bytes,"drw":drawings bytes,"fp":"image fingerprint","pins":[[sid,x,y],...]} with x/y
 * from 0 to 10000; fp is empty without an image. Pins for stations that no longer exist are left out.
 * With SITEMAP_DEVICE_IMAGE it also returns "loc":1: the apps keep the image.
 */
void server_json_sitemap(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	SiteMapHeader header = read_header();

	// Images stored before fingerprints existed are fingerprinted when the map is first read
	char fingerprint[SITEMAP_FINGERPRINT_LENGTH + 1] = "";
	if (header.image_size) {
		read_fingerprint(fingerprint);
#if !defined(SITEMAP_DEVICE_IMAGE)
		if (!fingerprint[0]) fingerprint_image(fingerprint);
#endif
	}

	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{\"rev\":$L,\"img\":$L,\"drw\":$L,\"fp\":\"$S\","), header.revision, header.image_size,
		current_file_size(SITEMAP_DRAWINGS_FILENAME), fingerprint);
#if defined(SITEMAP_DEVICE_IMAGE)
	bfill.emit_p(PSTR("\"loc\":1,"));
#endif
	bfill.emit_p(PSTR("\"pins\":["));

	if (header.pin_count) {
		os_file_type file = file_open(SITEMAP_FILENAME, FileOpenMode::Read);
		if (file) {
			bool comma = false;
			uint8_t pin[SITEMAP_PIN_SIZE];
			bool ok = file_seek(file, SITEMAP_HEADER_SIZE);
			for (uint16_t index = 0; ok && index < header.pin_count; index++) {
				if (file_read(file, pin, SITEMAP_PIN_SIZE) != (int)SITEMAP_PIN_SIZE) break;
				if (pin[0] >= os.nstations) continue;
				if (comma) bfill.emit_p(PSTR(","));
				comma = true;
				bfill.emit_p(PSTR("[$D,$D,$D]"), pin[0], get_u16(pin + 1), get_u16(pin + 3));
			}
			file_close(file);
		}
	}
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}

/** Replace the site map pins
 * Command: /cmp?pw=xxx with the pins as the request body: sid,x,y;sid,x,y;...
 * For small maps the pins may be sent as the query parameter p instead; an empty p clears all pins.
 */
void server_change_sitemap_pins(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;

	const char* source = req.getBody();
	size_t length = req.getBodyLength();
	if (!length) {
		source = req.getQueryParameter("p");
		if (!source) handle_return(HTML_DATA_MISSING);
		length = strlen(source);
	}

	uint8_t* pins = new (std::nothrow) uint8_t[MAX_NUM_STATIONS * SITEMAP_PIN_SIZE];
	if (!pins) handle_return(HTML_INTERNAL_ERROR);
	uint16_t count = 0;
	unsigned char result = parse_pins(source, length, pins, count);
	if (result == HTML_SUCCESS) {
		SiteMapHeader header = read_header();
		header.revision++;
		header.pin_count = count;
		if (!write_map(header, pins)) result = HTML_INTERNAL_ERROR;
	}
	delete[] pins;
	handle_return(result);
}

#if defined(SITEMAP_DEVICE_IMAGE)
/** Set the site map image the apps keep
 * Command: /cmi?pw=xxx&fp=fingerprint&sz=image bytes
 * fp is the first 16 bytes of the image's SHA-256 as 32 lowercase hex digits; fp empty with sz=0 removes the image.
 * Ends any transfer of the previous image between apps.
 */
void server_change_sitemap_image(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;

	const char* fingerprint = req.getQueryParameter("fp");
	uint32_t size = 0;
	if (!fingerprint) handle_return(HTML_DATA_MISSING);
	if (!parse_uint(req.getQueryParameter("sz"), SITEMAP_MAX_IMAGE_BYTES, size)) handle_return(HTML_DATA_OUTOFBOUND);
	if (fingerprint[0] ? !size || !is_fingerprint(fingerprint) : size != 0) handle_return(HTML_DATA_FORMATERROR);

	relay_state.reset();
	if (size ? !write_fingerprint(fingerprint) : !remove_file(SITEMAP_FINGERPRINT_FILENAME)) {
		handle_return(HTML_INTERNAL_ERROR);
	}
	SiteMapHeader header = read_header();
	header.revision++;
	header.image_size = size;
	handle_return(write_header(header) ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
}

uint8_t sitemap_relay_summary() {
	relay_state.expire(millis());
	return relay_state.summary();
}

/** Ask for the site map image from another app, or keep a place in the queue for it
 * Command: /mxr?pw=xxx&cid=client id&fp=fingerprint of the image wanted
 * Returns {"result":1,"st":state,"pos":queue position,"off":offset,"len":batch bytes,"tot":image bytes}.
 * pos 0 is the receiver being served; st 3 means a batch is ready to download with /mxd.
 */
void server_relay_join(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	uint32_t client = 0;
	unsigned char result = check_relay_request(req, client, true);
	if (result != HTML_SUCCESS) handle_return(result);
	SiteMapRelay::Status status = relay().join(client, millis());
	if (status.result != SiteMapRelay::SUCCESS) handle_return(status.result);

	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{\"result\":1,\"st\":$D,\"pos\":$D,\"off\":$L,\"len\":$L,\"tot\":$L}"), status.state,
		status.position, status.offset, status.length, status.total);
	handle_return(HTML_OK);
}

/** Offer to send the site map image to the app waiting for it, or keep sending it
 * Command: /mxs?pw=xxx&cid=client id&fp=fingerprint of the image the app has
 * Returns {"result":1,"st":state,"off":offset,"len":batch bytes,"win":batch size,"req":bytes per request,
 * "tot":image bytes}; while st is 2 the sender uploads bytes off+len up to off+win with /mxu. Result 0x30 means
 * another app is sending, 0x10 that nobody is waiting, and 0x40 that the controller has no room for a batch.
 */
void server_relay_claim(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	uint32_t client = 0;
	unsigned char result = check_relay_request(req, client, true);
	if (result != HTML_SUCCESS) handle_return(result);
	SiteMapRelay::Status status = relay().claim(client, millis());
	if (status.result != SiteMapRelay::SUCCESS) handle_return(status.result);

	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{\"result\":1,\"st\":$D,\"off\":$L,\"len\":$L,\"win\":$L,\"req\":$L,\"tot\":$L}"),
		status.state, status.offset, status.length, status.window, status.request, status.total);
	handle_return(HTML_OK);
}

/** Send part of the site map image to the app waiting for it
 * Command: /mxu?pw=xxx&cid=client id&off=offset, with up to req bytes base64 encoded as the request body,
 * or URL encoded in the d parameter (for OpenThings Cloud)
 */
void server_relay_upload(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	uint32_t client = 0;
	unsigned char result = check_relay_request(req, client, false);
	if (result != HTML_SUCCESS) handle_return(result);
	uint32_t offset = 0;
	if (!parse_uint(req.getQueryParameter("off"), SITEMAP_MAX_IMAGE_BYTES, offset)) handle_return(HTML_DATA_OUTOFBOUND);

	size_t encoded_length = 0;
	char* body = request_data(req, "d", encoded_length);
	uint32_t length = 0;
	if (!body) handle_return(HTML_DATA_MISSING);
	if (!decode_base64(body, encoded_length, length) || !length) handle_return(HTML_DATA_FORMATERROR);
	handle_return(relay().upload(client, offset, (const uint8_t*)body, length, millis()).result);
}

/** Download the batch of the site map image waiting for this app
 * Command: /mxd?pw=xxx&cid=client id
 * Returns the batch's bytes as application/octet-stream, starting at the offset /mxr reported.
 */
void server_relay_download(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	uint32_t client = 0;
	unsigned char result = check_relay_request(req, client, false);
	if (result != HTML_SUCCESS) handle_return(result);
	SiteMapRelay::Status status = relay().batch(client, millis());
	if (status.result != SiteMapRelay::SUCCESS) handle_return(status.result);
#if defined(ESP8266)
	if (!relay_storage.send(res, status.length)) handle_return(HTML_INTERNAL_ERROR);
#else
	if (!send_file(res, SITEMAP_RELAY_FILENAME, status.length, F("application/octet-stream"))) {
		handle_return(HTML_INTERNAL_ERROR);
	}
#endif
}

/** Confirm a batch of the site map image arrived, so the next one can be sent
 * Command: /mxa?pw=xxx&cid=client id&off=offset just past the batch
 */
void server_relay_confirm(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	uint32_t client = 0;
	unsigned char result = check_relay_request(req, client, false);
	if (result != HTML_SUCCESS) handle_return(result);
	uint32_t offset = 0;
	if (!parse_uint(req.getQueryParameter("off"), SITEMAP_MAX_IMAGE_BYTES, offset)) handle_return(HTML_DATA_OUTOFBOUND);
	handle_return(relay().confirm(client, offset, millis()).result);
}

/** Stop sending or waiting for the site map image
 * Command: /mxc?pw=xxx&cid=client id
 */
void server_relay_leave(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	uint32_t client = 0;
	unsigned char result = check_relay_request(req, client, false);
	if (result != HTML_SUCCESS) handle_return(result);
	handle_return(relay().leave(client, millis()).result);
}
#else
/** Upload one chunk of the site map image
 * Command: /um?pw=xxx&off=offset&tot=total size, with the chunk base64 encoded as the request body,
 * or URL encoded in the d parameter (for OpenThings Cloud). off and tot count decoded bytes. Chunks must arrive in order; off=0 starts a new upload,
 * and the last chunk written may be resent.
 * The image replaces the current one once the last chunk arrives and must be a JPEG.
 */
void server_upload_sitemap_image(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;

	uint32_t offset = 0;
	uint32_t total = 0;
	if (!parse_uint(req.getQueryParameter("off"), SITEMAP_MAX_IMAGE_BYTES, offset) ||
		!parse_uint(req.getQueryParameter("tot"), SITEMAP_MAX_IMAGE_BYTES, total)) {
		handle_return(HTML_DATA_OUTOFBOUND);
	}

	// Also rejects requests without data, such as CORS preflights
	size_t encoded_length = 0;
	char* body = request_data(req, "d", encoded_length);
	uint32_t length = 0;
	if (!body) handle_return(HTML_DATA_MISSING);
	if (!decode_base64(body, encoded_length, length) || !length) handle_return(HTML_DATA_FORMATERROR);

	if (!total || offset + length > total) handle_return(HTML_DATA_OUTOFBOUND);

	// The JPEG signature is checked on the first chunk, before anything is written
	if (offset == 0 && (length < 2 || (uint8_t)body[0] != 0xFF || (uint8_t)body[1] != 0xD8)) {
		handle_return(HTML_DATA_FORMATERROR);
	}
	unsigned char result = write_upload_chunk(SITEMAP_IMAGE_TEMP_FILENAME, SITEMAP_IMAGE_FILENAME, body, length,
		offset, total);
	if (result == HTML_OK) handle_return(HTML_SUCCESS);
	if (result != HTML_SUCCESS && result != UPLOAD_RESENT_LAST) handle_return(result);

	// A resent last chunk is no new change, unless the header missed the upload, as after a power cut
	SiteMapHeader header = read_header();
	if (result == UPLOAD_RESENT_LAST && header.image_size == total) handle_return(HTML_SUCCESS);
	char fingerprint[SITEMAP_FINGERPRINT_LENGTH + 1];
	fingerprint_image(fingerprint);
	header.revision++;
	header.image_size = total;
	handle_return(write_header(header) ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
}

/** Download the site map image
 * Command: /jmi?pw=xxx
 * Returns the JPEG, or result 0x10 when no image is stored.
 */
void server_json_sitemap_image(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;

	uint32_t size = read_header().image_size;
	if (!size || !send_file(res, SITEMAP_IMAGE_FILENAME, size, F("image/jpeg"))) handle_return(HTML_DATA_MISSING);
}
#endif

/** Upload one chunk of the site map drawings (head runs and drip lines)
 * Command: /umd?pw=xxx&off=offset&tot=total size, with the chunk as a plain text request body,
 * or URL encoded in the d parameter (for OpenThings Cloud)
 * Chunks must arrive in order and the last chunk written may be resent; off=0 starts a new upload, and tot=0 with an
 * empty body removes the drawings.
 * The text format belongs to the apps; it must be printable ASCII with newlines.
 */
void server_upload_sitemap_drawings(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;

	uint32_t offset = 0;
	uint32_t total = 0;
	if (!parse_uint(req.getQueryParameter("off"), SITEMAP_MAX_DRAWING_BYTES, offset) ||
		!parse_uint(req.getQueryParameter("tot"), SITEMAP_MAX_DRAWING_BYTES, total)) {
		handle_return(HTML_DATA_OUTOFBOUND);
	}

	size_t data_length = 0;
	const char* body = request_data(req, "d", data_length);
	uint32_t length = (uint32_t)data_length;
	if (total == 0 && offset == 0 && !body) {
		remove_file(SITEMAP_DRAWINGS_TEMP_FILENAME);
		if (!remove_file(SITEMAP_DRAWINGS_FILENAME)) handle_return(HTML_INTERNAL_ERROR);
		handle_return(bump_revision() ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
	}

	// Also rejects requests without data, such as CORS preflights
	if (!body) handle_return(HTML_DATA_MISSING);
	if (!is_drawing_text(body, length)) handle_return(HTML_DATA_FORMATERROR);
	unsigned char result = write_upload_chunk(SITEMAP_DRAWINGS_TEMP_FILENAME, SITEMAP_DRAWINGS_FILENAME, body, length,
		offset, total);
	if (result == HTML_OK || result == UPLOAD_RESENT_LAST) handle_return(HTML_SUCCESS);
	if (result != HTML_SUCCESS) handle_return(result);
	handle_return(bump_revision() ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
}

/** Download the site map drawings
 * Command: /jmd?pw=xxx
 * Returns the drawings text, or result 0x10 when none are stored.
 */
void server_json_sitemap_drawings(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	if (!send_file(res, SITEMAP_DRAWINGS_FILENAME, 0, F("text/plain"))) handle_return(HTML_DATA_MISSING);
}

/** Delete the site map pins, drawings and image (or its fingerprint)
 * Command: /dm?pw=xxx
 */
void server_delete_sitemap(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
#if defined(SITEMAP_DEVICE_IMAGE)
	relay_state.reset();
#endif
	remove_file(SITEMAP_IMAGE_TEMP_FILENAME);
	remove_file(SITEMAP_IMAGE_FILENAME);
	remove_file(SITEMAP_FINGERPRINT_FILENAME);
	remove_file(SITEMAP_DRAWINGS_TEMP_FILENAME);
	remove_file(SITEMAP_DRAWINGS_FILENAME);
	SiteMapHeader header = {read_header().revision + 1, 0, 0};
	handle_return(write_map(header, nullptr) ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
}
