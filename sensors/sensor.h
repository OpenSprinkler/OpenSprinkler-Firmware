#pragma once

// Sensor subsystem.
//
// Sensor is the abstract base for any periodic data source surfaced through
// the firmware's sensor list and adjustment pipeline. Concrete subclasses
// today include analog probes via ADS1115, weather-service inputs, and
// aggregates over other sensors; future ones may wrap GPIO inputs or
// system-internal signals (board temperature, RAM, etc.).

#include <stdint.h>
#include <cmath>
#include "util/utils.h"
#include "../defines.h"
#include "../bfiller.h"

#define SENSOR_NAME_LEN 33
#define SENSOR_CUSTOM_UNIT_LEN 9

// Serialized sensor records are: type, common length, subclass length,
// common payload, subclass payload. Payloads are append-only.
#define SENSOR_RECORD_HEADER_LEN 3

// Current common payload length:
// name[33] + unit[1] + interval[4] + flag[1] + min[4] + max[4] + uuid[2].
// Future common fields should be appended; new records carry their own common_len byte.
#define SENSOR_COMMON_PAYLOAD_LEN 49

#define SENSOR_UUID_NONE 0  // sentinel: "no sensor assigned" (0 = uninitialized/disabled)

// New-sensor defaults — single source of truth for both server_change_sensor and /jsd
#define SENSOR_DEFAULT_NAME             "New Sensor"
#define SENSOR_DEFAULT_INTERVAL         15
#define SENSOR_DEFAULT_UNIT             SensorUnit::Volt
#define SENSOR_DEFAULT_MIN              0
#define SENSOR_DEFAULT_MAX              5
#define SENSOR_DEFAULT_TYPE             1  // SensorType::ADS1115
#define SENSOR_DEFAULT_FLAG            (1 << SENSOR_FLAG_ENABLE)

// Two-level stringify so macro values expand before quoting (for use inside PSTR())
#define _SENSOR_DEFAULT_STR(x)   #x
#define SENSOR_DEFAULT_STR(x)    _SENSOR_DEFAULT_STR(x)

typedef struct {
	uint32_t interval;
	uint32_t next_update;
	float    value;
	uint16_t uuid;    // stable sensor identifier (0 = empty slot)
	uint8_t  flag;    // persistent config bits — synced from sens.dat on load
	uint8_t  status;  // runtime state bits    — never persisted, cleared on load
} sensor_memory_t;   // 16 bytes

// Runtime status bits (sensor_memory_t::status) — never persisted
#define SENSOR_STATUS_VALID        (1 << 0)  // has had at least one successful read
#define SENSOR_STATUS_ERROR        (1 << 1)  // last read attempt failed (hardware fault)
#define SENSOR_STATUS_STALE        (1 << 2)  // update window passed but read could not complete
#define SENSOR_STATUS_CLAMPED_HIGH (1 << 3)  // last value was clamped to max
#define SENSOR_STATUS_CLAMPED_LOW  (1 << 4)  // last value was clamped to min

// Sensor log file format — both structs are tightly packed (no padding) so
// sizeof() gives the exact on-disk byte count and offsetof() gives exact field offsets.
struct __attribute__((packed)) SensorLogHeader {
	uint8_t  magic;            // SENSOR_LOG_MAGIC
	uint8_t  version;          // SENSOR_LOG_VERSION
	uint16_t max_files;        // number of data files in rotation
	uint16_t records_per_file; // max records per data file
	uint16_t cur_file;         // index of the data file currently being written
	uint8_t  wrapped;          // 1 once all max_files slots have been used at least once
	uint8_t  reserved[7];
};  // 16 bytes

struct __attribute__((packed)) SensorLogRecord {
	uint32_t timestamp;    // unix epoch seconds
	float    value;        // sensor reading
	uint16_t uuid;         // sensor UUID; 0 marks a tombstone while preserving timestamp
};

enum class SensorType : uint8_t {
	Aggregate = 0,
	ADS1115,
	Weather,
	SystemInternal,
	OnboardDigital,
	MAX_VALUE,
};

// X(id, display_name)
#define SENSOR_UNIT_GROUP_LIST(X) \
	X(None,        "No Unit")    \
	X(Energy,      "Energy")      \
	X(Flow,        "Flow")        \
	X(Pressure,    "Pressure")    \
	X(Temperature, "Temperature") \
	X(Light,       "Light")       \
	X(Length,      "Length")      \
	X(Velocity,    "Velocity")    \
	X(Volume,      "Volume")      \
	X(Salinity,    "Salinity")    \
	X(Angle,       "Angle")       \
	X(Precipitation, "Precipitation") \
	X(SolarRadiation, "Solar Radiation")

// X(id, display_name, short_symbol, group_id)
#define SENSOR_UNIT_LIST(X) \
	X(None,              "None",               " ",     None)        \
	X(Percent,           "Percent",            "%",     None)        \
	X(PartsPerMillion,   "Parts Per Million",  "ppm",   None)        \
	X(Millivolt,         "Millivolt",          "mV",    Energy)      \
	X(Volt,              "Volt",               "V",     Energy)      \
	X(Milliampere,       "Milliampere",        "mA",    Energy)      \
	X(Ampere,            "Ampere",             "A",     Energy)      \
	X(Ohm,               "Ohm",                "Ω",     Energy)      \
	X(Milliohm,          "Milliohm",           "mΩ",    Energy)      \
	X(Kiloohm,           "Kiloohm",            "kΩ",    Energy)      \
	X(DielectricConstant,"Dielectric Constant"," ",     Energy)      \
	X(LitersPerSecond,   "Liters Per Second",  "L/s",   Flow)        \
	X(GallonsPerSecond,  "Gallons Per Second", "gal/s", Flow)        \
	X(Kilopascal,        "Kilopascal",         "kPa",   Pressure)    \
	X(Bar,               "Bar",                "bar",   Pressure)    \
	X(Pascal,            "Pascal",             "Pa",    Pressure)    \
	X(Torr,              "Torr",               "torr",  Pressure)    \
	X(Celsius,           "Celsius",            "°C",    Temperature) \
	X(Fahrenheit,        "Fahrenheit",         "°F",    Temperature) \
	X(Kelvin,            "Kelvin",             "K",     Temperature) \
	X(Lux,               "Lux",                "lx",    Light)       \
	X(Lumen,             "Lumen",              "lm",    Light)       \
	X(Millimeter,        "Millimeter",         "mm",    Length)      \
	X(Centimeter,        "Centimeter",         "cm",    Length)      \
	X(Meter,             "Meter",              "m",     Length)      \
	X(Kilometer,         "Kilometer",          "km",    Length)      \
	X(Inch,              "Inch",               "in",    Length)      \
	X(Foot,              "Foot",               "ft",    Length)      \
	X(Mile,              "Mile",               "mi",    Length)      \
	X(MetersPerSecond,   "Meters Per Second",  "m/s",   Velocity)    \
	X(KilometersPerHour, "Kilometers Per Hour","km/h",  Velocity)    \
	X(MilesPerHour,      "Miles Per Hour",     "mph",   Velocity)    \
	X(Milliliter,        "Milliliter",         "mL",    Volume)      \
	X(Liter,             "Liter",              "L",     Volume)      \
	X(CubicMeter,        "Cubic Meter",        "m³",    Volume)      \
	X(Gallon,            "Gallon",             "gal",   Volume)      \
	X(CubicFoot,         "Cubic Foot",         "ft³",   Volume)      \
	X(LitersPerMinute,   "Liters Per Minute",  "L/min", Flow)        \
	X(GallonsPerMinute,  "Gallons Per Minute", "gpm",   Flow)        \
	X(PoundsPerSquareInch, "Pounds Per Square Inch", "psi", Pressure) \
	X(VolumetricMoistureContent, "Volumetric Moisture Content", "%VMC", None) \
	X(Watt,              "Watt",               "W",     Energy)      \
	X(Milliwatt,         "Milliwatt",          "mW",    Energy)      \
	X(WattHour,          "Watt Hour",          "Wh",    Energy)      \
	X(KilowattHour,      "Kilowatt Hour",      "kWh",   Energy)      \
	X(MicrosiemensPerCentimeter, "Microsiemens Per Centimeter", "uS/cm", Salinity) \
	X(MillisiemensPerCentimeter, "Millisiemens Per Centimeter", "mS/cm", Salinity) \
	X(Ph,                "pH",                 "pH",    Salinity)    \
	X(Degree,            "Degree",             "deg",   Angle)       \
	X(Radian,            "Radian",             "rad",   Angle)       \
	X(MillimetersPerHour, "Millimeters Per Hour", "mm/h", Precipitation) \
	X(InchesPerHour,     "Inches Per Hour",    "in/h",  Precipitation) \
	X(Kilobyte,          "Kilobyte",           "KB",    None)        \
	X(MillimetersPerDay, "Millimeters Per Day", "mm/day", Precipitation) \
	X(InchesPerDay,      "Inches Per Day",      "in/day", Precipitation) \
	X(KilowattHoursPerSquareMeterPerDay, "Kilowatt Hours Per Square Meter Per Day", "kWh/m2/day", SolarRadiation)

enum class SensorUnitGroup : uint8_t {
#define X(id, name) id,
	SENSOR_UNIT_GROUP_LIST(X)
#undef X
	MAX_VALUE,
};

enum class SensorUnit : uint8_t {
#define X(id, name, sym, group) id,
	SENSOR_UNIT_LIST(X)
#undef X
	MAX_VALUE,
};

typedef enum {
	SENSOR_FLAG_ENABLE = 0,  // sensor is active
	SENSOR_FLAG_LOG,         // write readings to log file
	SENSOR_FLAG_SHOW,        // show on homepage
	SENSOR_FLAG_COUNT
} sensor_flag;

class Sensor {
public:
	Sensor(uint32_t interval, float min, float max, const char *name, SensorUnit unit, uint8_t flag);
	Sensor();
	virtual ~Sensor() {}

	float get_new_value(uint8_t *status_out = nullptr);
	uint32_t serialize(char *buf);

	static Sensor *parse(os_file_type file);         // statically allocated, do not delete
	static Sensor *get(uint8_t index);               // statically allocated, do not delete
	static bool    write(Sensor *sensor, uint8_t index);
	static void    load_count();
	static bool    save_count();
	static unsigned char add(Sensor *sensor);
	static unsigned char modify(uint8_t index, Sensor *sensor); // index is positional index
	static unsigned char del(uint8_t index); // index is positional index
	static void          load_all();
	static uint8_t       find_index(uint16_t uuid);
	static void          test_log(uint32_t n_records);

	void virtual emit_extra_json(BufferFiller *bfill) = 0;

	uint32_t interval = 1;
	float min = 0.f;
	float max = 0.f;
	uint8_t flag = 0;
	uint16_t uuid = 0;   // assigned by write_sensor on creation; 0 = not yet assigned
	SensorUnit unit = SensorUnit::None;
	char name[SENSOR_NAME_LEN] = {0};

	SensorType virtual get_sensor_type() = 0;

	private:
	float virtual _get_raw_value() = 0;
	protected:
	uint32_t _deserialize(char *buf, uint32_t len, uint8_t *subclass_len);
	uint32_t virtual _serialize_internal(char *buf) = 0;
};

// X(id, display_name)
#define AGGREGATE_ACTION_LIST(X) \
	X(Min,     "Min")     \
	X(Max,     "Max")     \
	X(Average, "Average") \
	X(Sum,     "Sum")     \
	X(Median,  "Median")  \
	X(Range,   "Range")

enum class AggregateAction : uint8_t {
#define X(id, name) id,
	AGGREGATE_ACTION_LIST(X)
#undef X
	MAX_VALUE,
};

typedef Sensor* (*SensorGetter)(uint8_t);

// X(id, display_name)
#define WEATHER_ACTION_LIST(X) \
	X(CurrentTemperature,       "Current Temperature")       \
	X(CurrentHumidity,          "Current Humidity")          \
	X(CurrentWindSpeed,         "Current Wind Speed")        \
	X(CurrentRaining,           "Currently Raining")         \
	X(ForecastLowTemperature,   "Today's Low Temperature")   \
	X(ForecastHighTemperature,  "Today's High Temperature")  \
	X(ForecastPrecipitation,    "Today's Precipitation")      \
	X(HistoricalTemperature,    "Previous Day Temperature")  \
	X(HistoricalHumidity,       "Previous Day Humidity")     \
	X(HistoricalPrecipitation,  "Previous Day Precipitation") \
	X(HistoricalWindSpeed,      "Previous Day Wind Speed")   \
	X(HistoricalSolarRadiation, "Previous Day Solar Radiation") \
	X(HistoricalETo,            "Previous Day ETo")

enum class WeatherAction : uint8_t {
#define X(id, name) id,
	WEATHER_ACTION_LIST(X)
#undef X
	MAX_VALUE,
};
static_assert(static_cast<uint8_t>(WeatherAction::MAX_VALUE) <= 16,
	"weather sensor valid mask must be widened");

typedef float (*WeatherGetter)(WeatherAction);

typedef struct {
	float x;
	float y;
} sensor_adjustment_point_t;

#define SENSOR_ADJUSTMENT_POINTS 8

typedef enum {
	SENADJ_FLAG_ENABLE = 0,
	SENADJ_FLAG_COUNT
} senadj_flag;

class SensorAdjustment {
public:
	SensorAdjustment(uint16_t uuid, uint8_t point_count, uint8_t flag, sensor_adjustment_point_t *points);

	static SensorAdjustment *read(uint8_t index, uint8_t nprograms); // returns statically allocated object, do not delete
	static bool              write(SensorAdjustment *adj, uint8_t index);

	float get_adjustment_factor(sensor_memory_t *sensors);

	uint16_t uuid;        // sensor UUID (SENSOR_UUID_NONE = adjustment disabled)
	uint8_t  point_count;
	uint8_t  flag;        // bit 0 (SENADJ_FLAG_ENABLE): enable sensor adjustment
	sensor_adjustment_point_t points[SENSOR_ADJUSTMENT_POINTS];
};

#define SENSOR_ADJUSTMENT_SIZE sizeof(SensorAdjustment)

// Serialization helpers used by all sensor types
template <typename T>
inline uint32_t write_buf(char* buf, T val) {
	memcpy(buf, &val, sizeof(val));
	return sizeof(val);
}

template <typename T>
inline bool read_buf(char* buf, uint32_t* i, uint32_t end, T& out) {
	bool ok = (*i + sizeof(T) <= end);
	if (ok) memcpy(&out, buf + *i, sizeof(T));
	*i += sizeof(T);
	return ok;
}

// Linear interpolation across (x, y) points, clamped at endpoints.
// Duplicate x values form a step (x == T returns the rightmost point at T).
// Caller is responsible for n >= 1 in normal use; n == 0 returns NAN.
float sensor_piecewise_interp(float x, const sensor_adjustment_point_t *points, uint8_t n);

// Convert a value between units within the same SensorUnitGroup.
// Returns the value unchanged if from == to or the units are in different groups.
// Supported conversions include Temperature, Length, Velocity, and
// Precipitation rate groups.
float convert_unit(float value, SensorUnit from, SensorUnit to);

const char *enum_string(SensorUnitGroup group);
const char *enum_string(AggregateAction action);
const char *enum_string(WeatherAction action);

const char* get_sensor_unit_name(SensorUnit unit);
const char* get_sensor_unit_short(SensorUnit unit);
const SensorUnitGroup get_sensor_unit_group(SensorUnit unit);
const uint32_t get_sensor_unit_index(SensorUnit unit);

// Sensor log file helpers
void         get_sensor_log_filename(char *buf, uint16_t file_no);
os_file_type open_sensor_log(uint16_t file_no, FileOpenMode mode);
os_file_type open_sensor_log_header(FileOpenMode mode);
void         remove_sensor_log(int16_t file_no = -1);  // -1 removes header + all data files
