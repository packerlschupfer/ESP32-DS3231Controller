# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-04

### Added
- Initial public release
- DS3231 RTC wrapper with advanced scheduling capabilities
- Support for up to 10 independent schedules with day-of-week masking
- Time range scheduling with midnight span support (e.g., 23:00-01:00)
- Vacation mode for temporary schedule disabling
- Pump exercise feature for monthly maintenance runs
- Alarm management (Alarm 1 and Alarm 2)
- Temperature monitoring from DS3231's built-in sensor
- Celsius and Fahrenheit temperature readings
- Event callbacks (schedule start/end, alarm events, time changes)
- Timezone-aware UTC conversion utilities
- NTP integration support (setTimeFromUTC)
- syncSystemTime() helper for RTC to system clock synchronization
- State persistence (serialize/deserialize for EEPROM/NVS)
- Thread-safe operations with RecursiveMutexGuard protection
- Initialization guards on all RTC-accessing methods
- Const-correct API with const overloads where appropriate
- I2C communication via Wire library
- dayOfWeekFromStr() accepts both short ("Sun") and long ("Sunday") names

### Technical Details
- Uses RecursiveMutexGuard for nested method call safety
- DS3231 has 1-second precision; syncSystemTime() sets tv_usec=0
- All public methods check initialization state before I2C access
- dayOfWeekStr() returns 3-letter abbreviations ("Sun", "Mon", etc.)
- formatDayMask() uses 2-letter abbreviations ("Su", "Mo", etc.)

Platform: ESP32 (Arduino framework)
Hardware: DS3231 RTC module (I2C)
License: GPL-3
Dependencies: Adafruit RTClib, ESP32-MutexGuard

### Notes
- Production-tested as NTP fallback and scheduler in industrial system
- Previous internal versions (v1.x) not publicly released
- Reset to v0.1.0 for clean public release start
