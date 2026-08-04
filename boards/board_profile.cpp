#include "board_profile.h"

#if defined(ESP8266)
#include <pgmspace.h>
#define OSBOARD_PROGMEM PROGMEM
#else
#define OSBOARD_PROGMEM
#endif

namespace osboard {
namespace {

const BoardProfile UNKNOWN_PROFILE OSBOARD_PROGMEM = {
	{{UNUSED_PIN, UNUSED_PIN, UNUSED_PIN},
	 {UNUSED_PIN, UNUSED_PIN, UNUSED_PIN, UNUSED_PIN},
	 UNUSED_PIN, UNUSED_PIN, UNUSED_PIN, UNUSED_PIN,
	 UNUSED_PIN, UNUSED_PIN, UNUSED_PIN, UNUSED_PIN},
	0,
	CAP_NONE,
};

const BoardProfile OS30_PROFILE OSBOARD_PROGMEM = {
	{{IO_EXPANDER_PIN_BASE + 1, 0, IO_EXPANDER_PIN_BASE + 3},
	 {12, 13, UNUSED_PIN, UNUSED_PIN},
	 14, 16, IO_EXPANDER_PIN_BASE + 6, IO_EXPANDER_PIN_BASE + 7,
	 UNUSED_PIN, UNUSED_PIN, UNUSED_PIN, UNUSED_PIN},
	2,
	CAP_NONE,
};

const BoardProfile OS31_PROFILE OSBOARD_PROGMEM = {
	{{IO_EXPANDER_PIN_BASE + 10, IO_EXPANDER_PIN_BASE + 11, IO_EXPANDER_PIN_BASE + 12},
	 {IO_EXPANDER_PIN_BASE + 8, IO_EXPANDER_PIN_BASE + 9, UNUSED_PIN, UNUSED_PIN},
	 14, 16, IO_EXPANDER_PIN_BASE + 13, IO_EXPANDER_PIN_BASE + 14,
	 IO_EXPANDER_PIN_BASE + 15, UNUSED_PIN, UNUSED_PIN, 12},
	2,
	CAP_NONE,
};

const BoardProfile OS32_33_PROFILE OSBOARD_PROGMEM = {
	{{2, 0, IO_EXPANDER_PIN_BASE + 12},
	 {3, 10, UNUSED_PIN, UNUSED_PIN},
	 UNUSED_PIN, 15, IO_EXPANDER_PIN_BASE + 13, IO_EXPANDER_PIN_BASE + 14,
	 UNUSED_PIN, IO_EXPANDER_PIN_BASE + 8, IO_EXPANDER_PIN_BASE + 15, UNUSED_PIN},
	2,
	CAP_ETHERNET,
};

const BoardProfile OS34_PROFILE OSBOARD_PROGMEM = {
	{{2, 0, IO_EXPANDER_PIN_BASE + 12},
	 {3, 10, IO_EXPANDER_PIN_BASE + 10, IO_EXPANDER_PIN_BASE + 11},
	 UNUSED_PIN, 15, IO_EXPANDER_PIN_BASE + 13, IO_EXPANDER_PIN_BASE + 14,
	 UNUSED_PIN, IO_EXPANDER_PIN_BASE + 8, IO_EXPANDER_PIN_BASE + 15, UNUSED_PIN},
	4,
	CAP_ETHERNET,
};

const BoardProfile OSPI_PROFILE OSBOARD_PROGMEM = {
	{{24, 18, 10},
	 {14, 23, UNUSED_PIN, UNUSED_PIN},
	 UNUSED_PIN, 15, UNUSED_PIN, UNUSED_PIN,
	 UNUSED_PIN, UNUSED_PIN, UNUSED_PIN, UNUSED_PIN},
	2,
	CAP_NONE,
};

const BoardProfile DEMO_PROFILE OSBOARD_PROGMEM = {
	{{0, 0, 0},
	 {0, 0, 0, 0},
	 0, 0, 0, 0, 0, 0, 0, 0},
	2,
	CAP_NONE,
};

#if defined(ESP8266)
BoardProfile active_profile = {};
#elif defined(OSPI)
const BoardProfile* active_profile = &OSPI_PROFILE;
#else
const BoardProfile* active_profile = &DEMO_PROFILE;
#endif

} // namespace

const BoardProfile& active() {
#if defined(ESP8266)
	return active_profile;
#else
	return *active_profile;
#endif
}

void select(ProfileId id) {
	const BoardProfile* profile;
	switch (id) {
	case PROFILE_OS_30:
		profile = &OS30_PROFILE;
		break;
	case PROFILE_OS_31:
		profile = &OS31_PROFILE;
		break;
	case PROFILE_OS_32_33:
		profile = &OS32_33_PROFILE;
		break;
	case PROFILE_OS_34:
		profile = &OS34_PROFILE;
		break;
	case PROFILE_OSPI_BOARD:
		profile = &OSPI_PROFILE;
		break;
	case PROFILE_DEMO_BOARD:
		profile = &DEMO_PROFILE;
		break;
	default:
		profile = &UNKNOWN_PROFILE;
		break;
	}
#if defined(ESP8266)
	memcpy_P(&active_profile, profile, sizeof(active_profile));
#else
	active_profile = profile;
#endif
}

bool has(Capability capability) {
	return (active().capabilities & capability) != 0;
}

} // namespace osboard
