# OpenSprinkler Enhanced Fertigation Feature

This document describes the enhanced fertigation (fertilizer + irrigation) feature implementation for OpenSprinkler with improved safety and centered timing.

## Overview

Fertigation allows automatic injection of fertilizers or nutrients into the irrigation system. A fertigation station acts like a master valve but only activates alongside the main station valve for a defined period or percentage of the station's runtime.

## Key Enhancements

### 🔒 Safety Features
- **Fertigation station protection**: Fertigation stations cannot be manually activated
- **Visual indicators**: LCD shows fertigation stations with 'F' indicator
- **Web interface warnings**: Clear warnings for protected stations
- **Master station compatibility**: Full integration with existing master valve system

### ⏱️ Centered Timing
- **Middle timing**: Fertigation runs in the middle of the station's watering time
- **Equal distribution**: Non-fertigation time is equally divided before and after
- **Smart calculation**: Automatic timing adjustment based on duration/percentage

### 🛡️ Enhanced Validation
- **Self-reference prevention**: Station cannot be its own fertigation station
- **Master station protection**: Master stations cannot be fertigation stations
- **Input validation**: Comprehensive parameter checking

## Timing Behavior

### Centered Fertigation Example
```
Station runs for 10 minutes, fertigation set to 2 minutes

Timeline:
0:00 - Station ON, Fertigation OFF
4:00 - Fertigation ON (starts in middle)
6:00 - Fertigation OFF (ends in middle)  
10:00 - Station OFF

Result: 4 minutes irrigation → 2 minutes fertigation → 4 minutes irrigation
```

### Calculation Logic
- **Time-based mode**: `delay = (station_duration - fertigation_duration) / 2`
- **Percentage-based mode**: `fertigation_duration = station_duration * percentage / 100`, then centered
- **Safety limits**: Fertigation duration cannot exceed station duration

## Implementation Summary

### Files Modified
1. **OpenSprinkler.h**: Added `is_fertigation_station()` function declaration
2. **OpenSprinkler.cpp**: 
   - Implemented fertigation station detection
   - Added protection in `set_station_bit()`
   - Enhanced `schedule_fertigation()` with centered timing
   - Added LCD 'F' indicator for fertigation stations
3. **main.cpp**: Updated fertigation scheduling calls
4. **opensprinkler_server.cpp**: Enhanced API validation
5. **fertigation.js**: Improved web interface with protection warnings

### Key Functions Added/Modified
- `is_fertigation_station()`: Detects if station is used as fertigation valve
- `schedule_fertigation()`: Now accepts station duration for centered timing
- `set_station_bit()`: Added protection against manual fertigation station activation
- LCD display: Added 'F' indicator for fertigation stations

## Usage Examples

### Basic Setup with Enhanced Safety
1. Configure Station 8 as a fertigation pump
2. Configure Station 1 for lawn irrigation:
   - Enable fertigation
   - Set mode to percentage (30%)
   - Set fertigation station to 8

**Result**: 
- Station 8 shows 'F' on LCD and cannot be manually activated
- When Station 1 runs for 10 minutes:
  - 0-3.5 min: Irrigation only
  - 3.5-6.5 min: Irrigation + Fertigation
  - 6.5-10 min: Irrigation only

### Web Interface Behavior
- **Regular stations**: Show fertigation settings button
- **Fertigation stations**: Show warning message and disabled controls
- **Settings dialog**: Includes timing explanations and validation

## Testing the Enhancements

### Test Fertigation Station Protection
```bash
# This should fail (fertigation station cannot be manually activated)
curl "http://localhost:8080/cm?sid=7&en=1&t=60"
```

### Test Centered Timing
```bash
# Configure station with 60-second fertigation
curl "http://localhost:8080/cf?sid=0&en=1&mode=0&dur=60&fsid=7"

# Run station for 300 seconds (5 minutes)
curl "http://localhost:8080/mf?sid=0&t=300&fert=1"

# Expected: Fertigation runs from 2:00 to 3:00 (centered)
```

### Test Validation
```bash
# This should fail (station cannot be its own fertigation station)
curl "http://localhost:8080/cf?sid=0&en=1&fsid=0"

# This should fail if station 1 is a master station
curl "http://localhost:8080/cf?sid=0&en=1&fsid=1"
```

## Migration Notes

Existing fertigation configurations will automatically benefit from:
- Centered timing instead of start-of-cycle timing
- Protection of fertigation stations from manual activation
- Visual indicators on LCD and web interface

No configuration changes are required for existing setups.

## Safety Benefits

1. **Prevents accidental activation**: Users cannot accidentally turn on fertilizer pumps
2. **Clear visual feedback**: Easy identification of fertigation stations
3. **System integrity**: Prevents invalid configurations
4. **Professional operation**: Behaves like commercial irrigation systems

## Future Enhancements

- Multiple fertigation intervals per station
- Soil moisture-based fertigation timing
- Fertigation usage tracking and reporting
- Advanced timing patterns (start/middle/end options)
- Integration with weather-based adjustments

This enhanced implementation provides a robust, safe, and professional fertigation system suitable for both residential and commercial applications.
