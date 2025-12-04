# DS3231Controller

A sophisticated DS3231 RTC wrapper library for ESP32 with advanced scheduling, alarm management, and hot water timer features.

## Features

- **Multiple Schedule Management**: Create and manage up to 10 independent schedules
- **Day-of-Week Scheduling**: Flexible scheduling with day mask support
- **Vacation Mode**: Temporarily disable schedules during vacation periods
- **Pump Exercise**: Prevent pump seizing with automatic monthly exercise
- **Temperature Monitoring**: Access the DS3231's built-in temperature sensor
- **Alarm Integration**: Automatic alarm setting for next scheduled events
- **Battery Backup Support**: Maintains time and schedules during power loss
- **Comprehensive Logging**: Built-in debug logging with custom logger support
- **Callback System**: Event notifications for schedule changes and alarms

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    packerlschupfer/DS3231Controller
    ; or for local development:
    ; DS3231Controller=symlink:///path/to/workspace_Class-DS3231Controller
```

### Arduino IDE

1. Download the library
2. Place in your Arduino libraries folder
3. Install dependency: Adafruit RTClib

## Hardware Requirements

- ESP32 board
- DS3231 RTC module
- I2C connection (default pins: SDA=21, SCL=22)

## Quick Start

```cpp
#include <DS3231Controller.h>

DS3231Controller rtc;

void setup() {
    Wire.begin();
    
    if (!rtc.begin()) {
        Serial.println("RTC init failed!");
        return;
    }
    
    // Create a schedule
    DS3231Controller::Schedule morning;
    morning.name = "Morning Heat";
    morning.startHour = 6;
    morning.startMinute = 0;
    morning.endHour = 8;
    morning.endMinute = 0;
    morning.dayMask = 0b01111110;  // Mon-Fri
    morning.enabled = true;
    
    rtc.addSchedule(morning);
}

void loop() {
    if (rtc.isWithinAnySchedule()) {
        // Activate heater/pump
    }
}
```

## Schedule Management

### Creating Schedules

```cpp
DS3231Controller::Schedule schedule;
schedule.name = "Evening Bath";
schedule.startHour = 18;
schedule.startMinute = 30;
schedule.endHour = 20;
schedule.endMinute = 0;
schedule.enabled = true;

// Set days (bit 0 = Sunday, bit 6 = Saturday)
schedule.dayMask = 0b11111111;  // Every day
// Or use helper methods:
schedule.setDay(1, true);  // Enable Monday
schedule.setDay(0, false); // Disable Sunday

rtc.addSchedule(schedule);
```

### Day Mask Values

- Sunday: bit 0 (0x01)
- Monday: bit 1 (0x02)
- Tuesday: bit 2 (0x04)
- Wednesday: bit 3 (0x08)
- Thursday: bit 4 (0x10)
- Friday: bit 5 (0x20)
- Saturday: bit 6 (0x40)

Common patterns:
- Weekdays: `0b01111110` (0x7E)
- Weekends: `0b10000001` (0x81)
- Every day: `0b11111111` (0xFF)

## Vacation Mode

```cpp
// Enable vacation for 7 days
DateTime now = rtc.now();
DateTime endDate = now + TimeSpan(7, 0, 0, 0);
rtc.setVacationMode(true, now, endDate);

// Check status
if (rtc.isVacationMode()) {
    Serial.println("On vacation!");
}
```

## Pump Exercise

Prevent pump seizing during extended off periods:

```cpp
// Run pump on 1st of each month at 3:00 AM for 5 minutes
rtc.setPumpExercise(true, 1, 3, 0, 300);

// Check if it's time
if (rtc.isPumpExerciseTime()) {
    // Run pump
    rtc.markPumpExerciseComplete();
}
```

## Temperature Monitoring

```cpp
// Get temperature data
auto tempData = rtc.getTemperature();
Serial.printf("Temperature: %.1f°C / %.1f°F\n", 
              tempData.celsius, tempData.fahrenheit);

// Or just Celsius
float temp = rtc.getTemperatureCelsius();
```

## Callbacks

```cpp
// Schedule events
rtc.onScheduleEvent([](const DS3231Controller::Schedule& schedule, bool isStart) {
    if (isStart) {
        Serial.printf("Schedule '%s' started\n", schedule.name.c_str());
    } else {
        Serial.printf("Schedule '%s' ended\n", schedule.name.c_str());
    }
});

// Alarm events
rtc.onAlarm([](uint8_t alarmNumber) {
    Serial.printf("Alarm %d fired!\n", alarmNumber);
});

// Time change events
rtc.onTimeChange([](const DateTime& newTime) {
    Serial.println("Time was changed");
});
```

## Persistence

Save and restore schedules to/from EEPROM or NVS:

```cpp
// Get required buffer size
size_t size = rtc.getScheduleDataSize();
uint8_t* buffer = new uint8_t[size];

// Serialize
if (rtc.serializeSchedules(buffer, size)) {
    // Save buffer to EEPROM/NVS
}

// Restore
if (rtc.deserializeSchedules(buffer, size)) {
    Serial.println("Schedules restored");
}

delete[] buffer;
```

## Debug Logging

Enable debug output:

```cpp
// In platformio.ini or build flags
build_flags = 
    -DDS3231_DEBUG
    -DUSE_CUSTOM_LOGGER  ; If using custom logger
```

## Examples

See the `examples` folder for:
- `basic` - Simple schedule example
- `hot_water_timer` - Complete hot water system controller

## API Reference

### Core Methods

- `begin(TwoWire* wire)` - Initialize the RTC
- `setTime(const DateTime& dt)` - Set RTC time
- `now()` - Get current time
- `getTemperature()` - Get temperature data

### Schedule Methods

- `addSchedule(schedule)` - Add a new schedule
- `updateSchedule(id, schedule)` - Update existing schedule
- `removeSchedule(id)` - Remove a schedule
- `getSchedule(id)` - Get schedule by ID
- `isWithinAnySchedule()` - Check if any schedule is active
- `getNextScheduledStart()` - Get next scheduled start time

### Utility Methods

- `printDiagnostics()` - Print debug information
- `getFormattedTime()` - Get time as "HH:MM:SS"
- `getFormattedDate()` - Get date as "YYYY-MM-DD"
- `getScheduleStatus()` - Get human-readable status

## License

MIT License - see LICENSE file for details

## Contributing

Pull requests are welcome! Please follow the existing code style and include tests for new features.

## Acknowledgments

- Built on top of Adafruit's RTClib
- Inspired by real-world hot water control systems