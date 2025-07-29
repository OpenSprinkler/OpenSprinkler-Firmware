# OpenSprinkler Fertigation Feature

This document describes the fertigation (fertilizer + irrigation) feature implementation for OpenSprinkler.

## Overview

Fertigation allows automatic injection of fertilizers or nutrients into the irrigation system. A fertigation station acts like a master valve but only activates alongside the main station valve for a defined period or percentage of the station's runtime.

## Features

- **Station-level configuration**: Each station can have individual fertigation settings
- **Program-level overrides**: Programs can override station fertigation settings
- **Two timing modes**:
  - **Time-based**: Fixed duration in seconds
  - **Percentage-based**: Percentage of main station runtime
- **Manual control**: Run stations with or without fertigation
- **Dedicated fertigation stations**: Any station can act as a fertigation valve

## API Endpoints

### Get Fertigation Data
```
GET /jf
```
Returns fertigation configuration for all stations.

**Response:**
```json
{
  "result": 1,
  "fertigation": [
    {
      "enabled": 1,
      "mode": 0,
      "duration": 60,
      "percentage": 30,
      "fert_sid": 7
    },
    ...
  ]
}
```

### Change Fertigation Settings
```
GET /cf?sid=<station_id>&en=<enabled>&mode=<mode>&dur=<duration>&pct=<percentage>&fsid=<fert_station_id>
```

**Parameters:**
- `sid`: Station index (0-based)
- `en`: Enable fertigation (0 or 1)
- `mode`: Fertigation mode (0=time, 1=percentage)
- `dur`: Duration in seconds (for time mode)
- `pct`: Percentage (for percentage mode)
- `fsid`: Fertigation station ID

### Manual Fertigation Run
```
GET /mf?sid=<station_id>&t=<duration>&fert=<enable_fert>
```

**Parameters:**
- `sid`: Station index
- `t`: Duration in seconds
- `fert`: Enable fertigation (0 or 1)

## Data Structures

### Station Data
Each station can store fertigation configuration in its special data area:
```c
struct FertigationStationData {
    unsigned char enabled;           // Fertigation enabled
    unsigned char mode;              // 0=time, 1=percentage
    uint16_t duration;               // Duration in seconds
    unsigned char percentage;        // Percentage 0-100
    unsigned char fertigation_sid;   // Fertigation station ID
};
```

### Program Data
Programs can override station fertigation settings:
```c
struct ProgramStruct {
    // ... existing fields ...
    uint16_t fertigation_durations[MAX_NUM_STATIONS]; // Per-station overrides
};
```

## Implementation Details

### Core Functions
- `get_fertigation_data()`: Retrieve station fertigation settings
- `set_fertigation_data()`: Update station fertigation settings
- `schedule_fertigation()`: Schedule fertigation for a station
- `process_fertigation()`: Process fertigation queue (called from main loop)
- `calculate_fertigation_time()`: Calculate fertigation duration

### Integration Points
- **Station Control**: Modified `set_station_bit()` to trigger fertigation
- **Program Execution**: Enhanced `turn_on_station()` for program-based fertigation
- **Main Loop**: Added `process_fertigation()` call
- **API Server**: Added new endpoints for fertigation control

## Usage Examples

### Basic Setup
1. Configure Station 8 as a fertigation pump
2. Configure Station 1 for lawn irrigation with fertigation:
   - Enable fertigation
   - Set mode to percentage
   - Set percentage to 30%
   - Set fertigation station to 8

### Program Override
Create a program that overrides Station 1 fertigation to 45 seconds instead of percentage-based.

### Manual Operation
Run Station 1 for 10 minutes with fertigation enabled.

## Configuration

### Station Configuration
Each station can be configured with:
- Fertigation enabled/disabled
- Timing mode (time or percentage)
- Duration (for time mode) or percentage (for percentage mode)
- Which station acts as the fertigation valve

### Program Configuration
Programs can override station fertigation settings with custom durations for each station.

## Safety Features

- Fertigation automatically stops when main station stops
- Maximum duration and percentage limits
- Validation of fertigation station IDs
- Queue management to prevent conflicts

## Web Interface

The web interface includes:
- Fertigation settings dialog for each station
- Manual run controls with fertigation option
- Program editor with fertigation overrides
- Real-time status display

## Testing

To test the fertigation feature:

1. **Firmware Testing**:
   ```bash
   cd OpenSprinkler-Firmware
   make clean && make
   ./OpenSprinkler
   ```

2. **Web Interface Testing**:
   ```bash
   cd OpenSprinkler-App
   npm start
   ```

3. **API Testing**:
   ```bash
   # Get fertigation data
   curl "http://localhost:8080/jf"
   
   # Configure station 0 with fertigation
   curl "http://localhost:8080/cf?sid=0&en=1&mode=0&dur=60&fsid=7"
   
   # Manual run with fertigation
   curl "http://localhost:8080/mf?sid=0&t=300&fert=1"
   ```

## Future Enhancements

- Multiple fertigation stations per main station
- Fertigation scheduling based on soil conditions
- Integration with weather data for fertigation adjustments
- Fertigation usage logging and reporting
- Mobile app support for fertigation controls
