#ifndef _ROOMS_H
#define _ROOMS_H

#include "types.h"
#include "OpenSprinkler.h"

class BufferFiller; // Forward declaration

#define MAX_NUM_ROOMS 8
#define ROOM_NAME_SIZE 24

struct RoomStruct {
    char name[ROOM_NAME_SIZE];
    int16_t time_offset_minutes; // Offset in minutes (can be negative)
    bool spray_mode;             // Spray mode active?
    uint8_t station_bits[4];     // Bitmask for up to 32 stations (assuming max 32 for now, or match os.nstations)
                                 // Note: OpenSprinkler supports many stations. 4 bytes = 32 stations.
                                 // If more are needed, we can increase this.
                                 // For now, let's just use a simplified list or mask.
                                 // Actually, let's use a simpler approach: just store the mask for now.
                                 // Or better, since we have dynamic station counts, let's use a helper.
};

// JSON file to store room data
#define ROOMS_FILENAME "/rooms.json"

class RoomManager {
public:
    static RoomStruct rooms[MAX_NUM_ROOMS];
    static uint8_t room_count;

    static void init();
    static void load();
    static void save();

    static void set_room(uint8_t id, const char* name, int16_t offset, bool spray);
    static int16_t get_offset(uint8_t id);
    static bool is_spray_mode(uint8_t id);

    // Helpers
    static bool parse_room_tag(const char* prog_name, int8_t* room_id, int8_t* phase);
    static void create_room_tag(char* buffer, uint8_t room_id, uint8_t phase);

    // API Helpers
    static void to_json(BufferFiller& bfill);
    static void apply_preset(uint8_t room_id, const char* preset_name);

    // Logging
    static void log_change(const char* msg);
};

#endif
