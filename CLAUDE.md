# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

### Build and Development
```bash
# Build the library examples
cd examples/basic && pio run
cd examples/hot_water_timer && pio run

# Upload to ESP32
pio run -t upload

# Monitor serial output
pio device monitor -b 115200

# Enable debug logging
# Add to platformio.ini build_flags:
# -DDS3231_DEBUG
```

### Code Quality
```bash
# No specific linting or type checking commands are configured
# The library follows Arduino/ESP32 conventions
```

## Architecture Overview

This is a DS3231 RTC wrapper library for ESP32 that provides advanced scheduling capabilities for hot water timer systems and similar time-based automation.

### Core Components

1. **DS3231Controller Class** (`src/DS3231Controller.h/cpp`)
   - Main controller providing high-level RTC operations
   - Manages up to 10 independent schedules with day-of-week masking
   - Implements vacation mode and pump exercise features
   - Handles alarm setting for next scheduled events
   - Provides temperature monitoring from DS3231's sensor

2. **Data Structures**
   - `Schedule`: Time ranges with day mask (bit 0=Sunday, bit 6=Saturday)
   - `PumpExercise`: Monthly maintenance runs to prevent pump seizing
   - `VacationMode`: Temporary schedule disabling with date range
   - `TemperatureData`: Celsius/Fahrenheit temperature readings

3. **Logging System** (`src/DS3231ControllerLogging.h`)
   - Conditional compilation for ESP-IDF or custom logger
   - Debug logging enabled via `DS3231_DEBUG` flag
   - Integrates with external logger submodule when `USE_CUSTOM_LOGGER` is defined

### Key Design Patterns

1. **Event-Driven Architecture**
   - Callbacks for schedule start/end events
   - Alarm event notifications
   - Time change notifications

2. **Schedule Management**
   - Schedules can span midnight (e.g., 23:00 to 01:00)
   - Day masks use bit flags for flexible day selection
   - Automatic next alarm calculation

3. **State Persistence**
   - Serialize/deserialize methods for EEPROM/NVS storage
   - Maintains settings through power cycles

### Development Considerations

- Library depends on Adafruit RTClib for core RTC operations
- Uses Arduino framework on ESP32 platform
- I2C communication via Wire library (default pins: SDA=21, SCL=22)
- Power loss sets RTC to compile time automatically

### Timezone Handling

The DS3231 RTC stores time without timezone awareness. Important considerations:

1. **RTC Storage**: The DS3231 stores whatever time you give it (typically local time)
2. **DateTime Constructor**: `DateTime(epochTime)` interprets epoch as UTC
3. **System Integration**: ESP32 system time uses timezone via `setenv("TZ", ...)`

#### Recommended Approach for Timezone Systems:

```cpp
// Setting RTC from NTP (UTC):
int32_t tzOffset = 3600; // CET = UTC+1 in seconds
rtc.setTimeFromUTC(ntpEpochTime, tzOffset);

// Reading RTC for system time:
uint32_t utcTime = rtc.nowUTC(tzOffset);
```

#### Common Pitfall:
```cpp
// WRONG - stores UTC time in RTC, but RTC thinks it's local
rtc.setTime(DateTime(ntpEpochUTC));

// CORRECT - converts to local time first
rtc.setTimeFromUTC(ntpEpochUTC, timezoneOffsetSeconds);
```

### Testing Approach

- No unit tests; rely on example applications for integration testing
- `printDiagnostics()` method provides runtime verification
- Hot water timer example includes interactive serial command interface
- Debug logging throughout for troubleshooting

## Development Workflow

- Commit after changes so the project using the lib can test the latest version