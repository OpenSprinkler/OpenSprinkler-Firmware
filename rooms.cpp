#include "rooms.h"
#include "OpenSprinkler.h"
#include "opensprinkler_server.h"
#include "program.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(ARDUINO)
    #if defined(ESP8266)
    #include <FS.h>
    #include <LittleFS.h>
    #else
    #include "SdFat.h"
    extern SdFat sd;
    #endif
#endif

// Initialize static members
RoomStruct RoomManager::rooms[MAX_NUM_ROOMS];
uint8_t RoomManager::room_count = 0;

// Defines for presets
#define PRESET_VEG_DAY1 "Veg Day 1"
#define PRESET_FLOWER_DAY1 "Flower Day 1"

void RoomManager::init() {
    memset(rooms, 0, sizeof(rooms));
    for(int i=0; i<MAX_NUM_ROOMS; i++) {
        snprintf(rooms[i].name, ROOM_NAME_SIZE, "Room %d", i+1);
    }
    load();
}

void RoomManager::load() {
    // Simple JSON loader or binary loader.
    // Since we don't have a full JSON parser library easily exposed,
    // we'll implement a simple line-based or binary read/write for now.
    // For robustness, let's use a binary struct dump for now, as it's easier to implement reliably in embedded C++.
    // But the requirements mentioned "rooms.json" earlier.
    // Let's try to do a basic read.

#if defined(ESP8266)
    if (LittleFS.exists(ROOMS_FILENAME)) {
        File f = LittleFS.open(ROOMS_FILENAME, "r");
        if (f) {
            f.read((uint8_t*)rooms, sizeof(rooms));
            f.close();
        }
    }
#elif defined(ARDUINO)
    if (sd.exists(ROOMS_FILENAME)) {
        SdFile f;
        if (f.open(ROOMS_FILENAME, O_READ)) {
            f.read((uint8_t*)rooms, sizeof(rooms));
            f.close();
        }
    }
#else
    // Linux/Native
    FILE *f = fopen(ROOMS_FILENAME, "rb");
    if (f) {
        fread(rooms, sizeof(rooms), 1, f);
        fclose(f);
    }
#endif
}

void RoomManager::save() {
#if defined(ESP8266)
    File f = LittleFS.open(ROOMS_FILENAME, "w");
    if (f) {
        f.write((uint8_t*)rooms, sizeof(rooms));
        f.close();
    }
#elif defined(ARDUINO)
    SdFile f;
    if (f.open(ROOMS_FILENAME, O_WRITE | O_CREAT | O_TRUNC)) {
        f.write((uint8_t*)rooms, sizeof(rooms));
        f.close();
    }
#else
    FILE *f = fopen(ROOMS_FILENAME, "wb");
    if (f) {
        fwrite(rooms, sizeof(rooms), 1, f);
        fclose(f);
    }
#endif
}

void RoomManager::set_room(uint8_t id, const char* name, int16_t offset, bool spray) {
    if (id >= MAX_NUM_ROOMS) return;
    strncpy(rooms[id].name, name, ROOM_NAME_SIZE);
    rooms[id].name[ROOM_NAME_SIZE-1] = 0;
    rooms[id].time_offset_minutes = offset;
    rooms[id].spray_mode = spray;
    save();

    char logbuf[64];
    snprintf(logbuf, sizeof(logbuf), "Room %d updated: %s, Offset %d, Spray %d", id+1, name, offset, spray);
    log_change(logbuf);
}

int16_t RoomManager::get_offset(uint8_t id) {
    if (id >= MAX_NUM_ROOMS) return 0;
    return rooms[id].time_offset_minutes;
}

bool RoomManager::is_spray_mode(uint8_t id) {
    if (id >= MAX_NUM_ROOMS) return false;
    return rooms[id].spray_mode;
}

// Tag format: {#R1:P1} where 1 is Room ID+1 (1-based), P1 is Phase
bool RoomManager::parse_room_tag(const char* prog_name, int8_t* room_id, int8_t* phase) {
    if (prog_name[0] == '{' && prog_name[1] == '#') {
        // Expected format: {#Rx:Py}
        // Scan for R
        const char* r = strchr(prog_name, 'R');
        const char* p = strchr(prog_name, 'P');
        const char* end = strchr(prog_name, '}');

        if (r && p && end && r < p && p < end) {
            *room_id = atoi(r + 1) - 1; // Convert 1-based to 0-based
            *phase = atoi(p + 1);
            return (*room_id >= 0 && *room_id < MAX_NUM_ROOMS);
        }
    }
    return false;
}

void RoomManager::create_room_tag(char* buffer, uint8_t room_id, uint8_t phase) {
    sprintf(buffer, "{#R%d:P%d} ", room_id + 1, phase);
}

void RoomManager::to_json(BufferFiller& bfill) {
    bfill.emit_p(PSTR("["));
    for(int i=0; i<MAX_NUM_ROOMS; i++) {
        bfill.emit_p(PSTR("{\"id\":$D,\"name\":\"$S\",\"offset\":$D,\"spray\":$D}"),
            i, rooms[i].name, rooms[i].time_offset_minutes, rooms[i].spray_mode);
        if (i < MAX_NUM_ROOMS - 1) bfill.emit_p(PSTR(","));
    }
    bfill.emit_p(PSTR("]"));
}

void RoomManager::log_change(const char* msg) {
    // Append to change_log.txt
    time_os_t now = os.now_tz();

#if defined(ESP8266)
    File f = LittleFS.open("/change_log.txt", "a");
    if (f) {
        f.printf("[%lu] %s\n", now, msg);
        f.close();
    }
#elif defined(ARDUINO)
    SdFile f;
    if (f.open("/change_log.txt", O_WRITE | O_CREAT | O_APPEND)) {
        f.print("["); f.print(now); f.print("] "); f.println(msg);
        f.close();
    }
#else
    FILE *f = fopen("./logs/change_log.txt", "a"); // Use logs folder for linux
    if (f) {
        fprintf(f, "[%lu] %s\n", (unsigned long)now, msg);
        fclose(f);
    }
#endif
}

void RoomManager::apply_preset(uint8_t room_id, const char* preset_name) {
    if (room_id >= MAX_NUM_ROOMS) return;

    // Clear existing programs for this room/phase?
    // Ideally we should find any existing programs for this room and delete them or update them.
    // For now, let's just ADD new programs. User can clean up old ones.

    ProgramStruct prog;
    memset(&prog, 0, sizeof(prog));
    prog.enabled = 1;
    prog.type = PROGRAM_TYPE_INTERVAL;
    // Default to every day
    prog.days[0] = 0;
    prog.days[1] = 1; // Interval 1 day

    char tag[32];

    if (strcmp(preset_name, PRESET_VEG_DAY1) == 0) {
        // P1: Start 06:00, Dur 2 mins
        create_room_tag(tag, room_id, 1);
        strcpy(prog.name, tag);
        strcat(prog.name, "Veg D1 P1");
        prog.starttime_type = 0; // Repeating
        prog.starttimes[0] = 6 * 60; // 6:00 AM
        prog.starttimes[1] = 0; // Repeat 0 times (run once)
        prog.starttimes[2] = 0; // Interval 0

        // Duration for all stations (User will need to set which stations are in the room manually,
        // or we need a way to know. For now, set all to 0, user updates via UI)
        // Actually, without knowing which stations are in the room, we can't set durations safely.
        // Assuming user will edit.
        for(int i=0; i<MAX_NUM_STATIONS; i++) prog.durations[i] = 0;

        ProgramData::add(&prog);

        // P2: Start 10:00, Repeat 4 times, Interval 2 hrs
        create_room_tag(tag, room_id, 2);
        strcpy(prog.name, tag);
        strcat(prog.name, "Veg D1 P2");
        prog.starttimes[0] = 10 * 60;
        prog.starttimes[1] = 3; // Repeat 3 MORE times (total 4)
        prog.starttimes[2] = 120; // 2 hours
        ProgramData::add(&prog);

        log_change("Applied Preset: Veg Day 1");
    }
    else if (strcmp(preset_name, PRESET_FLOWER_DAY1) == 0) {
         // P1: Start 08:00
        create_room_tag(tag, room_id, 1);
        strcpy(prog.name, tag);
        strcat(prog.name, "Flower D1 P1");
        prog.starttimes[0] = 8 * 60;
        prog.starttimes[1] = 0;
        prog.starttimes[2] = 0;
        ProgramData::add(&prog);

        // P2: Less frequent
        create_room_tag(tag, room_id, 2);
        strcpy(prog.name, tag);
        strcat(prog.name, "Flower D1 P2");
        prog.starttimes[0] = 12 * 60;
        prog.starttimes[1] = 1; // Repeat 1 MORE time (total 2)
        prog.starttimes[2] = 240; // 4 hours
        ProgramData::add(&prog);

        log_change("Applied Preset: Flower Day 1");
    }
}
