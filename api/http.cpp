#include "http.h"

#include "../OpenSprinkler.h"

#include <cstring>

extern char tmp_buffer[];
extern char ether_buffer[];
extern OpenSprinkler os;

namespace {

OTF::Response* current_response = nullptr;

void bfill_flush(const char* buffer, size_t length) {
	if (current_response && length > 0) current_response->writeBodyData(buffer, length);
}

} // namespace

BufferFiller bfill;

void begin_response(OTF::Response& response) {
	current_response = &response;
	bfill = BufferFiller(ether_buffer, ETHER_BUFFER_SIZE);
	bfill.set_flush(bfill_flush);
}

uint16_t findKeyVal(const OTF::Request& request, char* buffer, uint16_t max_length,
	const char* key, bool key_in_program_memory, uint8_t* key_found) {
#if defined(ARDUINO)
	char* result = key_in_program_memory
		? request.getQueryParameter((const __FlashStringHelper*)key)
		: request.getQueryParameter(key);
#else
	char* result = request.getQueryParameter(key);
#endif
	if (result) {
		strncpy(buffer, result, max_length);
		buffer[max_length - 1] = 0;
		if (key_found) *key_found = 1;
		return strlen(buffer);
	}
	if (key_found) *key_found = 0;
	return 0;
}

uint16_t findKeyVal(const char* source, char* buffer, uint16_t max_length,
	const char* key, bool key_in_program_memory, uint8_t* key_found) {
	uint8_t found = 0;
	uint16_t length = 0;
	if (!source || !buffer || !key) return 0;

	const char* key_position = key;
	if (key_in_program_memory) {
		while (*source && *source != ' ' && *source != '\n' && !found) {
			if (*source == pgm_read_byte(key_position)) {
				key_position++;
				if (pgm_read_byte(key_position) == '\0') {
					source++;
					key_position = key;
					if (*source == '=') found = 1;
				}
			} else {
				key_position = key;
			}
			source++;
		}
	} else {
		while (*source && *source != ' ' && *source != '\n' && !found) {
			if (*source == *key_position) {
				key_position++;
				if (*key_position == '\0') {
					source++;
					key_position = key;
					if (*source == '=') found = 1;
				}
			} else {
				key_position = key;
			}
			source++;
		}
	}

	if (found) {
		while (*source && *source != ' ' && *source != '\n' && *source != '&' &&
			length < max_length - 1) {
			*buffer++ = *source++;
			length++;
		}
		if (!*source || *source == ' ' || *source == '\n' || *source == '&') {
			*buffer = '\0';
		} else {
			found = 0;
			length = 0;
		}
	}
	if (key_found) *key_found = found;
	return length;
}

void print_header(const OTF::Request&, OTF::Response& response,
	ContentType content_type, int content_length) {
	response.writeStatus(200, F("OK"));
	switch (content_type) {
	case CT_JSON:
		response.writeHeader(F("Content-Type"), F("application/json"));
		break;
	case CT_HTML:
		response.writeHeader(F("Content-Type"), F("text/html"));
		break;
	case CT_CSV:
		response.writeHeader(F("Content-Type"), F("text/csv"));
		break;
	case CT_BINARY:
		response.writeHeader(F("Content-Type"), F("application/octet-stream"));
		break;
	}
	if (content_length > 0) response.writeHeader(F("Content-Length"), content_length);
	response.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	response.writeHeader(F("Cache-Control"), F("max-age=0, no-cache, no-store, must-revalidate"));
	response.writeHeader(F("Connection"), F("close"));
}

void print_header_compressed_html(const OTF::Request&, OTF::Response& response,
	int content_length) {
	response.writeStatus(200, F("OK"));
	response.writeHeader(F("Content-Type"), F("text/html; charset=utf-8"));
	response.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	response.writeHeader(F("Content-Length"), content_length);
	response.writeHeader(F("Vary"), F("Accept-Encoding"));
	response.writeHeader(F("Content-Encoding"), F("gzip"));
	response.writeHeader(F("Connection"), F("close"));
}

void otf_send_result(const OTF::Request& request, OTF::Response& response,
	unsigned char code, const char* item) {
	String json = F("{\"result\":");
#if defined(ARDUINO)
	json += code;
#else
	json += std::to_string(code);
#endif
	if (!item) item = "";
	json += F(",\"item\":\"");
	json += item;
	json += F("\"}");
	print_header(request, response, CT_JSON, json.length());
	response.writeBodyChunk((char*)"%s", json.c_str());
}

bool process_password(const OTF::Request& request, OTF::Response& response,
	bool firmware_version_on_failure) {
#if defined(DEMO)
	return true;
#endif
	if (os.iopts[IOPT_IGNORE_PASSWORD]) return true;
	const char* password = request.getQueryParameter("pw");
	if (password && os.password_verify(password)) return true;

	if (firmware_version_on_failure) {
		print_header(request, response);
		begin_response(response);
		iopt_get_json_name(IOPT_FW_VERSION, tmp_buffer);
		bfill.emit_p(PSTR("{\"$S\":$D}"), tmp_buffer, os.iopts[0]);
	} else {
		otf_send_result(request, response, HTML_UNAUTHORIZED);
	}
	return false;
}
