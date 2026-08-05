#pragma once

#include "../bfiller.h"
#include "OpenThingsFramework.h"

#include <cstdint>

enum ContentType {
	CT_JSON,
	CT_HTML,
	CT_CSV,
	CT_BINARY,
};

constexpr unsigned char HTML_OK = 0x00;
constexpr unsigned char HTML_SUCCESS = 0x01;
constexpr unsigned char HTML_UNAUTHORIZED = 0x02;
constexpr unsigned char HTML_MISMATCH = 0x03;
constexpr unsigned char HTML_DATA_MISSING = 0x10;
constexpr unsigned char HTML_DATA_OUTOFBOUND = 0x11;
constexpr unsigned char HTML_DATA_FORMATERROR = 0x12;
constexpr unsigned char HTML_RFCODE_ERROR = 0x13;
constexpr unsigned char HTML_PAGE_NOT_FOUND = 0x20;
constexpr unsigned char HTML_NOT_PERMITTED = 0x30;
constexpr unsigned char HTML_UPLOAD_FAILED = 0x40;
constexpr unsigned char HTML_INTERNAL_ERROR = 0x50;
constexpr unsigned char HTML_REDIRECT_HOME = 0xFF;

extern BufferFiller bfill;

void begin_response(OTF::Response& response);

unsigned char findKeyVal(const OTF::Request& request, char* buffer, uint16_t max_length,
	const char* key, bool key_in_program_memory = false, uint8_t* key_found = nullptr);
unsigned char findKeyVal(const char* source, char* buffer, uint16_t max_length,
	const char* key, bool key_in_program_memory = false, uint8_t* key_found = nullptr);

void print_header(const OTF::Request& request, OTF::Response& response,
	ContentType content_type = CT_JSON, int content_length = 0);
void print_header_compressed_html(const OTF::Request& request, OTF::Response& response,
	int content_length);
void otf_send_result(const OTF::Request& request, OTF::Response& response,
	unsigned char code, const char* item = nullptr);
bool process_password(const OTF::Request& request, OTF::Response& response,
	bool firmware_version_on_failure = false);
