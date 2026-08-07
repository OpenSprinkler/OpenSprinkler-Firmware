#pragma once

#include <stdint.h>

#if defined(ARDUINO)
	#include <Arduino.h>

enum class FirmwareUpdatePhase : uint8_t {
	Idle,
	Prepared,
	Uploading,
	Verifying,
	Success,
	Error
};

class FirmwareUpdateService {
public:
	bool configured() const;
	const char *target() const;
	const char *file_extension() const;
	const char *base_url() const;

	bool issue_token(const char *password, char token_out[17]);
	bool consume_token(const char *token);

	bool prepare_verified(const uint8_t *descriptor, size_t descriptor_length,
		const char *signature_hex, const char *release_id, bool allow_downgrade,
		char upload_token_out[17]);
	bool begin_verified(const char *filename, const char *upload_token);
	bool write_verified(const uint8_t *data, size_t length);
	bool finish_verified();

	bool begin_manual(const char *filename, uint32_t size, const char *sha256_hex);
	bool write_manual(const uint8_t *data, size_t length);
	bool finish_manual();
	void abort_upload();
	void loop();

	FirmwareUpdatePhase phase() const { return _phase; }
	const char *phase_name() const;
	const char *message() const { return _message; }
	uint32_t bytes_done() const { return _bytes_done; }
	uint32_t bytes_total() const { return _bytes_total; }
	uint8_t percent() const;
	bool busy() const;
	bool display_active() const;

private:
	bool begin_image(const uint8_t header[16], uint32_t size);
	bool valid_image_header(const uint8_t header[16]) const;
	bool begin_upload(const char *filename, bool verified, uint32_t size = 0);
	bool write_upload(const uint8_t *data, size_t length, bool verified);
	bool finish_upload(bool verified);
	void fail(const char *message);
	void reset_transfer();
	void update_display();
	void make_token(char token_out[17]);

	FirmwareUpdatePhase _phase = FirmwareUpdatePhase::Idle;
	char _message[96] = "Ready";
	char _token[17] = {};
	char _upload_token[17] = {};
	uint32_t _token_expires = 0;
	uint32_t _upload_token_expires = 0;
	uint32_t _bytes_done = 0;
	uint32_t _bytes_total = 0;
	uint32_t _last_progress = 0;
	uint32_t _reboot_at = 0;
	uint32_t _display_until = 0;
	uint8_t _expected_sha256[32] = {};
	uint8_t _image_header[16] = {};
	uint8_t _image_header_len = 0;
	uint8_t _tail_byte = 0;
	bool _tail_pending = false;
	bool _update_started = false;
	bool _manual = false;
};

extern FirmwareUpdateService firmware_update;
#endif
