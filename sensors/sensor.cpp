#include "sensor.h"
#include "../OpenSprinkler.h"

extern OpenSprinkler os;
extern char tmp_buffer[];

const char *enum_string(SensorUnitGroup group) {
	switch (group) {
	#define X(id, name) case SensorUnitGroup::id: return PSTR(name);
	SENSOR_UNIT_GROUP_LIST(X)
	#undef X
	case SensorUnitGroup::MAX_VALUE: return nullptr;
	}
	return nullptr;
}

const char *enum_string(AggregateAction action) {
	switch (action) {
	#define X(id, name) case AggregateAction::id: return PSTR(name);
	AGGREGATE_ACTION_LIST(X)
	#undef X
	case AggregateAction::MAX_VALUE: return nullptr;
	}
	return nullptr;
}

const char *enum_string(WeatherAction action) {
	switch (action) {
	#define X(id, name) case WeatherAction::id: return PSTR(name);
	WEATHER_ACTION_LIST(X)
	#undef X
	case WeatherAction::MAX_VALUE: return nullptr;
	}
	return nullptr;
}

const char* get_sensor_unit_name(SensorUnit unit) {
	switch (unit) {
	#define X(id, name, sym, group) case SensorUnit::id: return PSTR(name);
	SENSOR_UNIT_LIST(X)
	#undef X
	case SensorUnit::MAX_VALUE: return nullptr;
	}
	return nullptr;
}

const char* get_sensor_unit_short(SensorUnit unit) {
	switch (unit) {
	#define X(id, name, sym, group) case SensorUnit::id: return PSTR(sym);
	SENSOR_UNIT_LIST(X)
	#undef X
	case SensorUnit::MAX_VALUE: return nullptr;
	}
	return nullptr;
}

const SensorUnitGroup get_sensor_unit_group(SensorUnit unit) {
	switch (unit) {
	#define X(id, name, sym, group) case SensorUnit::id: return SensorUnitGroup::group;
	SENSOR_UNIT_LIST(X)
	#undef X
	case SensorUnit::MAX_VALUE: return SensorUnitGroup::MAX_VALUE;
	}
	return SensorUnitGroup::MAX_VALUE;
}

const uint32_t get_sensor_unit_index(SensorUnit unit) {
	return static_cast<uint32_t>(unit);
}

static bool sensor_has_valid_layout(char *buf, uint32_t slot_size, uint32_t *len_out) {
	if ((uint8_t)buf[0] >= (uint8_t)SensorType::MAX_VALUE) return false;
	uint32_t len = SENSOR_RECORD_HEADER_LEN + (uint8_t)buf[1] + (uint8_t)buf[2];
	if (len < SENSOR_RECORD_HEADER_LEN || len > slot_size) return false;
	if (len_out) *len_out = len;
	return true;
}

Sensor::Sensor(uint32_t interval, float min, float max, const char* name, SensorUnit unit, uint8_t flag) :
	interval(interval), min(min), max(max), flag(flag), unit(unit) {
	strncpy(this->name, name, SENSOR_NAME_LEN);
	this->name[SENSOR_NAME_LEN - 1] = 0;
}

Sensor::Sensor() {}

float Sensor::get_new_value(uint8_t *status_out) {
	float value = this->_get_raw_value();

	if (isnan(value)) {
		if (status_out) *status_out = SENSOR_STATUS_ERROR;
		return value;
	}

	uint8_t status = SENSOR_STATUS_VALID;
	if (value < this->min) { value = this->min; status |= SENSOR_STATUS_CLAMPED_LOW; }
	if (value > this->max) { value = this->max; status |= SENSOR_STATUS_CLAMPED_HIGH; }
	if (status_out) *status_out = status;

	return value;
}

uint32_t Sensor::serialize(char* buf) {
	uint32_t i = 0;

	buf[i++] = static_cast<uint8_t>(this->get_sensor_type());
	uint32_t common_len_pos = i++;
	uint32_t subclass_len_pos = i++;

	uint32_t common_start = i;
	memcpy(buf + i, this->name, SENSOR_NAME_LEN);
	i += SENSOR_NAME_LEN;
	buf[i++] = static_cast<uint8_t>(this->unit);
	i += write_buf(buf + i, this->interval);
	i += write_buf(buf + i, this->flag);
	i += write_buf(buf + i, this->min);
	i += write_buf(buf + i, this->max);
	i += write_buf(buf + i, this->uuid);
	buf[common_len_pos] = static_cast<uint8_t>(i - common_start);

	uint32_t subclass_start = i;
	i += this->_serialize_internal(buf + i);
	buf[subclass_len_pos] = static_cast<uint8_t>(i - subclass_start);
	return i;
}

uint32_t Sensor::_deserialize(char* buf, uint32_t len, uint8_t *subclass_len) {
	uint8_t common_len = static_cast<uint8_t>(buf[1]);
	*subclass_len     = static_cast<uint8_t>(buf[2]);

	uint32_t i = SENSOR_RECORD_HEADER_LEN;
	uint32_t common_end = i + common_len;

	if (i + SENSOR_NAME_LEN <= common_end) {
		memcpy(this->name, buf + i, SENSOR_NAME_LEN);
		this->name[SENSOR_NAME_LEN - 1] = 0;
	}
	i += SENSOR_NAME_LEN;

	if (i + 1 <= common_end) this->unit = static_cast<SensorUnit>(buf[i]);
	i++;

	read_buf(buf, &i, common_end, this->interval);
	read_buf(buf, &i, common_end, this->flag);
	read_buf(buf, &i, common_end, this->min);
	read_buf(buf, &i, common_end, this->max);
	read_buf(buf, &i, common_end, this->uuid);

	return common_end;
}

float convert_unit(float value, SensorUnit from, SensorUnit to) {
	if (from == to) return value;
	SensorUnitGroup g = get_sensor_unit_group(from);
	if (g != get_sensor_unit_group(to)) return value;   // incompatible groups: no-op

	if (g == SensorUnitGroup::Temperature) {
		// Pivot through Celsius
		float c;
		switch (from) {
			case SensorUnit::Celsius:    c = value; break;
			case SensorUnit::Fahrenheit: c = (value - 32.0f) * 5.0f / 9.0f; break;
			case SensorUnit::Kelvin:     c = value - 273.15f; break;
			default:                     return value;
		}
		switch (to) {
			case SensorUnit::Celsius:    return c;
			case SensorUnit::Fahrenheit: return c * 9.0f / 5.0f + 32.0f;
			case SensorUnit::Kelvin:     return c + 273.15f;
			default:                     return value;
		}
	}
	if (g == SensorUnitGroup::Length) {
		float meters;
		switch (from) {
			case SensorUnit::Millimeter: meters = value / 1000.0f; break;
			case SensorUnit::Centimeter: meters = value / 100.0f; break;
			case SensorUnit::Meter:      meters = value; break;
			case SensorUnit::Kilometer:  meters = value * 1000.0f; break;
			case SensorUnit::Inch:       meters = value * 0.0254f; break;
			case SensorUnit::Foot:       meters = value * 0.3048f; break;
			case SensorUnit::Mile:       meters = value * 1609.344f; break;
			default: return value;
		}
		switch (to) {
			case SensorUnit::Millimeter: return meters * 1000.0f;
			case SensorUnit::Centimeter: return meters * 100.0f;
			case SensorUnit::Meter:      return meters;
			case SensorUnit::Kilometer:  return meters / 1000.0f;
			case SensorUnit::Inch:       return meters / 0.0254f;
			case SensorUnit::Foot:       return meters / 0.3048f;
			case SensorUnit::Mile:       return meters / 1609.344f;
			default: return value;
		}
	}
	if (g == SensorUnitGroup::Velocity) {
		float meters_per_second;
		switch (from) {
			case SensorUnit::MetersPerSecond:   meters_per_second = value; break;
			case SensorUnit::KilometersPerHour: meters_per_second = value / 3.6f; break;
			case SensorUnit::MilesPerHour:      meters_per_second = value * 0.44704f; break;
			default: return value;
		}
		switch (to) {
			case SensorUnit::MetersPerSecond:   return meters_per_second;
			case SensorUnit::KilometersPerHour: return meters_per_second * 3.6f;
			case SensorUnit::MilesPerHour:      return meters_per_second / 0.44704f;
			default: return value;
		}
	}
	if (g == SensorUnitGroup::Precipitation) {
		float millimeters_per_hour;
		switch (from) {
			case SensorUnit::MillimetersPerHour: millimeters_per_hour = value; break;
			case SensorUnit::InchesPerHour:      millimeters_per_hour = value * 25.4f; break;
			case SensorUnit::MillimetersPerDay:  millimeters_per_hour = value / 24.0f; break;
			case SensorUnit::InchesPerDay:       millimeters_per_hour = value * 25.4f / 24.0f; break;
			default: return value;
		}
		switch (to) {
			case SensorUnit::MillimetersPerHour: return millimeters_per_hour;
			case SensorUnit::InchesPerHour:      return millimeters_per_hour / 25.4f;
			case SensorUnit::MillimetersPerDay:  return millimeters_per_hour * 24.0f;
			case SensorUnit::InchesPerDay:       return millimeters_per_hour * 24.0f / 25.4f;
			default: return value;
		}
	}
	return value;
}

float sensor_piecewise_interp(float x, const sensor_adjustment_point_t *points, uint8_t n) {
	if (n == 0) return NAN;
	// duplicate x values form a step: x == T maps to the rightmost point at T
	if (x < points[0].x) return points[0].y;
	if (x >= points[n - 1].x) return points[n - 1].y;

	uint8_t i = 0;
	while (i + 1 < n - 1 && x >= points[i + 1].x) i++;

	const sensor_adjustment_point_t &left  = points[i];
	const sensor_adjustment_point_t &right = points[i + 1];

	if (right.x == left.x) return right.y;

	return (x - left.x) / (right.x - left.x) * (right.y - left.y) + left.y;
}

SensorAdjustment::SensorAdjustment(uint16_t uuid, uint8_t point_count, uint8_t flag, sensor_adjustment_point_t* points) {
	this->uuid = uuid;
	this->flag = flag;
	memset(this->points, 0, sizeof(this->points));
	if (point_count > SENSOR_ADJUSTMENT_POINTS) point_count = SENSOR_ADJUSTMENT_POINTS;
	this->point_count = point_count;
	for (size_t i = 0; i < point_count; i++) {
		this->points[i] = points[i];
	}
}

float SensorAdjustment::get_adjustment_factor(sensor_memory_t* sensors) {
	if (!sensors || this->uuid == SENSOR_UUID_NONE ||
		!(this->flag & (1 << SENADJ_FLAG_ENABLE)) || this->point_count == 0) return 1.f;

	uint8_t idx = Sensor::find_index(this->uuid);
	if (idx >= OpenSprinkler::nsensors) return 1.f;

	const sensor_memory_t &sensor = sensors[idx];
	if (!sensor.interval || !(sensor.flag & (1 << SENSOR_FLAG_ENABLE)) ||
		!(sensor.status & SENSOR_STATUS_VALID) ||
		(sensor.status & (SENSOR_STATUS_ERROR | SENSOR_STATUS_STALE))) return 1.f;

	float value = sensor_piecewise_interp(sensor.value, this->points, this->point_count);
	if (value < 0) value = 0;   // adjustment factor must be non-negative
	return value;
}

SensorAdjustment *SensorAdjustment::read(uint8_t index, uint8_t nprograms) {
	static SensorAdjustment result(SENSOR_UUID_NONE, 0, 0, nullptr);

	if (index >= nprograms) return nullptr;
	os_file_type file = file_open(SENADJ_FILENAME, FileOpenMode::Read);
	if (file) {
		uint32_t pos = (uint32_t)index * SENSOR_ADJUSTMENT_SIZE;
		file_seek(file, pos, FileSeekMode::Set);
		bool ok = (file_read(file, &result, SENSOR_ADJUSTMENT_SIZE) == (int)SENSOR_ADJUSTMENT_SIZE);
		file_close(file);
		if (ok && result.uuid != SENSOR_UUID_NONE) {
			return &result;
		}
	}
	return nullptr;
}

bool SensorAdjustment::write(SensorAdjustment *adj, uint8_t index) {
	uint32_t pos = (uint32_t)SENSOR_ADJUSTMENT_SIZE * index;
	SensorAdjustment disabled(SENSOR_UUID_NONE, 0, 0, nullptr);
	os_file_type file = file_open(SENADJ_FILENAME, FileOpenMode::Read);
	uint32_t cur_size = file ? file_size(file) : 0;
	if (file) file_close(file);
	while (cur_size < pos) {
		if (!file_write_block(SENADJ_FILENAME, &disabled, cur_size, SENSOR_ADJUSTMENT_SIZE)) return false;
		cur_size += SENSOR_ADJUSTMENT_SIZE;
	}
	SensorAdjustment *to_write = adj ? adj : &disabled;
	return file_write_block(SENADJ_FILENAME, to_write, pos, SENSOR_ADJUSTMENT_SIZE);
}

// ---------------------------------------------------------------------------
// Sensor file I/O
// ---------------------------------------------------------------------------

#include "aggregate_sensor.h"
#include "weather_sensor.h"
#include "ads1115_sensor.h"

static void sensor_memory_init(sensor_memory_t &m, Sensor *sensor) {
	m.interval = sensor->interval;
	m.flag = static_cast<uint8_t>(sensor->flag);
	m.uuid = sensor->uuid;
	m.next_update = 0;
	m.value = 0.f;
	m.status = 0;
}

Sensor *Sensor::parse(os_file_type file) {
	alignas(OpenSprinkler::SensorUnion)
	static uint8_t sensor_scratchpad[sizeof(OpenSprinkler::SensorUnion)];
	static Sensor *active_sensor = nullptr;

	if (active_sensor != nullptr) {
		active_sensor->~Sensor();
		active_sensor = nullptr;
	}
	const uint32_t slot_size = TMP_BUFFER_SIZE;
	memset(tmp_buffer, 0, slot_size);
	int bytes_read = file_read(file, tmp_buffer, slot_size);
	if (bytes_read < (int)SENSOR_RECORD_HEADER_LEN) return nullptr;

	uint32_t len = 0;
	if (!sensor_has_valid_layout((char*)tmp_buffer, (uint32_t)bytes_read, &len)) return nullptr;

	SensorType sensor_type = static_cast<SensorType>(tmp_buffer[0]);

	switch (sensor_type) {
		case SensorType::Aggregate:
			active_sensor = new (sensor_scratchpad) AggregateSensor(os.sensors, (char*)tmp_buffer, len);
			break;
		case SensorType::ADS1115:
			active_sensor = new (sensor_scratchpad) ADS1115Sensor(os.ads1115_devices, (char*)tmp_buffer, len);
			break;
		case SensorType::Weather:
			active_sensor = new (sensor_scratchpad) WeatherSensor(os.get_sensor_weather_data, (char*)tmp_buffer, len);
			break;
		case SensorType::SystemInternal:
			active_sensor = new (sensor_scratchpad) SystemInternalSensor((char*)tmp_buffer, len);
			break;
		case SensorType::OnboardDigital:
			active_sensor = new (sensor_scratchpad) OnboardDigitalSensor((char*)tmp_buffer, len);
			break;
		default:
			return nullptr;
	}
	return active_sensor;
}

Sensor *Sensor::get(uint8_t index) {
	const uint32_t slot_size = TMP_BUFFER_SIZE;
	uint32_t pos = 1 + slot_size * index;

	os_file_type file = file_open(SENSORS_FILENAME, FileOpenMode::Read);
	if (file) {
		file_seek(file, pos, FileSeekMode::Set);
		Sensor *result = Sensor::parse(file);
		file_close(file);
		return result;
	} else {
		DEBUG_PRINT("Failed to open file: ");
		DEBUG_PRINTLN(SENSORS_FILENAME);
		return nullptr;
	}
}

bool Sensor::write(Sensor *sensor, uint8_t index) {
	const uint32_t slot_size = TMP_BUFFER_SIZE;
	uint32_t pos = 1 + slot_size * index;

	memset(tmp_buffer, 0, slot_size);
	if (sensor) sensor->serialize(tmp_buffer);

	return file_write_block(SENSORS_FILENAME, tmp_buffer, pos, slot_size);
}

void Sensor::load_count() {
	OpenSprinkler::nsensors = file_read_byte(SENSORS_FILENAME, 0);
	if (OpenSprinkler::nsensors > MAX_SENSORS) {
		OpenSprinkler::nsensors = MAX_SENSORS;
	}
}

bool Sensor::save_count() {
	return file_write_byte(SENSORS_FILENAME, 0, OpenSprinkler::nsensors);
}

unsigned char Sensor::add(Sensor *sensor) {
	if (OpenSprinkler::nsensors >= MAX_SENSORS) return 0;

	if (!Sensor::write(sensor, OpenSprinkler::nsensors)) return 0;
	if (!file_write_byte(SENSORS_FILENAME, 0, OpenSprinkler::nsensors + 1)) return 0;
	sensor_memory_init(OpenSprinkler::sensors[OpenSprinkler::nsensors], sensor);
	OpenSprinkler::nsensors++;
	return 1;
}

unsigned char Sensor::modify(uint8_t index, Sensor *sensor) {
	if (index >= OpenSprinkler::nsensors) return 0;
	if (!Sensor::write(sensor, index)) return 0;
	sensor_memory_init(OpenSprinkler::sensors[index], sensor);
	return 1;
}

unsigned char Sensor::del(uint8_t index) {
	if (index >= OpenSprinkler::nsensors) return 0;
	if (OpenSprinkler::nsensors == 0) return 0;

	const uint32_t slot_size = TMP_BUFFER_SIZE;
	// erase by copying backward
	for (uint8_t i = index; i < OpenSprinkler::nsensors - 1; i++) {
		if (!file_copy_block(SENSORS_FILENAME, 1 + (uint32_t)(i + 1) * slot_size,
			1 + (uint32_t)i * slot_size, slot_size, tmp_buffer)) return 0;
	}
	if (!file_write_byte(SENSORS_FILENAME, 0, OpenSprinkler::nsensors - 1)) return 0;
	for (uint8_t i = index; i < OpenSprinkler::nsensors - 1; i++) {
		OpenSprinkler::sensors[i] = OpenSprinkler::sensors[i + 1];
	}
	OpenSprinkler::nsensors--;
	OpenSprinkler::sensors[OpenSprinkler::nsensors].interval = 0;
	OpenSprinkler::sensors[OpenSprinkler::nsensors].uuid = 0;

	return 1;
}

void Sensor::load_all() {
	if (!file_exists(SENSORS_FILENAME)) {
		DEBUG_PRINTLN(F("Sensor files missing. Initializing..."));
		OpenSprinkler::nsensors = 0;
		Sensor::save_count();
	} else {
		Sensor::load_count();
	}

	Sensor *sensor;
	bool repair_count = false;
	os_file_type file = file_open(SENSORS_FILENAME, FileOpenMode::Read);
	if (file) {
		uint32_t size = file_size(file);
		uint32_t complete_records = size > 1 ? (size - 1) / TMP_BUFFER_SIZE : 0;
		if (OpenSprinkler::nsensors > complete_records) {
			OpenSprinkler::nsensors = (uint8_t)complete_records;
			repair_count = true;
		}
		file_seek(file, 1, FileSeekMode::Set);
		for (size_t i = 0; i < OpenSprinkler::nsensors; i++) {
			if ((sensor = Sensor::parse(file)))
				sensor_memory_init(OpenSprinkler::sensors[i], sensor);
		}

		file_close(file);
	} else {
		DEBUG_PRINT("Failed to open file: ");
		DEBUG_PRINTLN(SENSORS_FILENAME);
	}
	if (repair_count) Sensor::save_count();

	// Stagger initial poll times by 1 second per sensor so that on first boot
	// we don't fire all sensor reads simultaneously. Sensor i first polls at
	// ~i seconds after this point; subsequent polls follow each sensor's own
	// interval, preserving the offset across cycles.
	uint32_t now_ms = millis();
	for (uint8_t i = 0; i < OpenSprinkler::nsensors; i++) {
		if (OpenSprinkler::sensors[i].interval) {
			OpenSprinkler::sensors[i].next_update = now_ms + (uint32_t)i * 1000;
		}
	}
}

uint8_t Sensor::find_index(uint16_t uuid) {
	if (uuid == SENSOR_UUID_NONE) return OpenSprinkler::nsensors;
	for (uint8_t i = 0; i < OpenSprinkler::nsensors; i++) {
		if (OpenSprinkler::sensors[i].uuid == uuid) return i;
	}
	return MAX_SENSORS;
}

void Sensor::test_log(uint32_t n_records) {
	remove_sensor_log();

	DEBUG_PRINTF("sensor log test: writing %lu records\n", (unsigned long)n_records);
	uint32_t t0 = millis();

	for (uint32_t i = 0; i < n_records; i++) {
		if (OpenSprinkler::nsensors > 0) os.log_sensor((uint8_t)((i + 1) % OpenSprinkler::nsensors), (float)i / 1000.f);
	}

	uint32_t write_ms = millis() - t0;
	(void)write_ms;
	DEBUG_PRINTF("sensor log write: %lu ms total, %.2f ms/record\n",
		(unsigned long)write_ms,
		n_records ? (float)write_ms / n_records : 0.f);

	os_file_type hfile = open_sensor_log_header(FileOpenMode::Read);
	if (!hfile) {
		DEBUG_PRINTLN("sensor log test: cannot open header");
		return;
	}
	SensorLogHeader hdr = {};
	file_read(hfile, &hdr, sizeof(hdr));
	file_close(hfile);
	if (hdr.magic != SENSOR_LOG_MAGIC || hdr.version != SENSOR_LOG_VERSION) {
		DEBUG_PRINTLN("sensor log test: bad header");
		return;
	}

	uint16_t first_file  = hdr.wrapped ? (uint16_t)((hdr.cur_file + 1) % hdr.max_files) : 0;
	uint16_t total_files = hdr.wrapped ? hdr.max_files : (uint16_t)(hdr.cur_file + 1);
	DEBUG_PRINTF("sensor log state: max_files=%u records_per_file=%u cur_file=%u wrapped=%u total_files=%u\n",
		hdr.max_files, hdr.records_per_file, hdr.cur_file, hdr.wrapped, total_files);

	uint32_t tr = millis();
	uint32_t count = 0;
	for (uint16_t fi = 0; fi < total_files; fi++) {
		uint16_t file_no = (first_file + fi) % hdr.max_files;
		os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
		if (!dfile) continue;
		SensorLogRecord rec;
		while (file_read(dfile, &rec, sizeof(rec)) == (int)sizeof(rec)) count++;
		file_close(dfile);
	}

	uint32_t read_ms = millis() - tr;
	(void)read_ms;
	DEBUG_PRINTF("sensor log read: %lu records in %lu ms (%.2f ms/record)\n",
		(unsigned long)count, (unsigned long)read_ms,
		count ? (float)read_ms / count : 0.f);
}

// ---------------------------------------------------------------------------
// Sensor log file helpers
// ---------------------------------------------------------------------------

void get_sensor_log_filename(char *buf, uint16_t file_no) {
	snprintf(buf, 24, "%s%03u", SENSORS_LOG_FILENAME, file_no % 1000);
}

os_file_type open_sensor_log(uint16_t file_no, FileOpenMode mode) {
	char fname[24];
	get_sensor_log_filename(fname, file_no);
	return file_open(fname, mode);
}

os_file_type open_sensor_log_header(FileOpenMode mode) {
	return file_open(SENSORS_LOG_HEADER_FILENAME, mode);
}

void remove_sensor_log(int16_t file_no) {
	char fname[24];
	if (file_no < 0) {
		remove_file(SENSORS_LOG_HEADER_FILENAME);
		for (uint16_t i = 0; i < SENSOR_LOG_MAX_FILES; i++) {
			get_sensor_log_filename(fname, i);
			remove_file(fname);
		}
	} else {
		get_sensor_log_filename(fname, (uint16_t)file_no);
		remove_file(fname);
	}
}
