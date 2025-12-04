#include "DS3231Controller.h"
#include <sys/time.h>

DS3231Controller::DS3231Controller()
    : _activeScheduleId(0), _mutex(nullptr) {
    _mutex = xSemaphoreCreateRecursiveMutex();
    _vacationMode.enabled = false;
    _pumpExercise.enabled = false;
    _pumpExercise.dayOfMonth = 1;
    _pumpExercise.hour = 3;
    _pumpExercise.minute = 0;
    _pumpExercise.durationSeconds = 300;
}

DS3231Controller::~DS3231Controller() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

bool DS3231Controller::begin(TwoWire* wire) {
    // Prevent double initialization (which causes "Bus already started" warnings)
    if (_initialized) {
        DS3231_LOG_D("DS3231 already initialized - skipping");
        return true;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for begin()");
        return false;
    }

    DS3231_LOG_I("Initializing DS3231 RTC controller");

    if (!_rtc.begin(wire)) {
        DS3231_LOG_E("Failed to initialize DS3231");
        return false;
    }

    if (_rtc.lostPower()) {
        DS3231_LOG_W("RTC lost power, setting to compile time");
        // Set to compile time as fallback
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Clear any pending alarms
    _rtc.clearAlarm(1);
    _rtc.clearAlarm(2);

    _lastCheck = _rtc.now();
    _initialized = true;

    DS3231_LOG_I("DS3231 initialized successfully. Current time: %s",
                 _lastCheck.timestamp(DateTime::TIMESTAMP_FULL).c_str());

    return true;
}

bool DS3231Controller::isRunning() const {
    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }
    // Check if oscillator is running
    DateTime now = _rtc.now();
    return now.isValid();
}

bool DS3231Controller::setTime(const DateTime& dt) {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return false;
    }

    if (!dt.isValid()) {
        DS3231_LOG_E("Invalid DateTime provided");
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for setTime()");
        return false;
    }

    DS3231_LOG_D("Setting RTC time to: %s", dt.timestamp(DateTime::TIMESTAMP_FULL).c_str());
    _rtc.adjust(dt);

    if (_timeChangeCallback) {
        _timeChangeCallback(dt);
    }

    return true;
}

DateTime DS3231Controller::now() const {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return DateTime();  // Return invalid DateTime
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for now()");
        return DateTime();  // Return invalid DateTime
    }

    return _rtc.now();
}

bool DS3231Controller::addSchedule(const Schedule& schedule) {
    if (_schedules.size() >= MAX_SCHEDULES) {
        DS3231_LOG_E("Maximum number of schedules (%d) reached", MAX_SCHEDULES);
        return false;
    }
    
    Schedule newSchedule = schedule;
    if (newSchedule.id == 0) {
        newSchedule.id = getNextFreeScheduleId();
    }
    
    _schedules.push_back(newSchedule);
    
    DS3231_LOG_I("Added schedule %d '%s': %02d:%02d-%02d:%02d, days=%s", 
                 newSchedule.id, newSchedule.name.c_str(),
                 newSchedule.startHour, newSchedule.startMinute,
                 newSchedule.endHour, newSchedule.endMinute,
                 formatDayMask(newSchedule.dayMask).c_str());
    
    // Update alarm for next event
    (void)setAlarmForNextSchedule();
    
    return true;
}

bool DS3231Controller::updateSchedule(uint8_t scheduleId, const Schedule& schedule) {
    for (auto& sched : _schedules) {
        if (sched.id == scheduleId) {
            sched = schedule;
            sched.id = scheduleId;  // Preserve ID
            DS3231_LOG_I("Updated schedule %d", scheduleId);
            (void)setAlarmForNextSchedule();
            return true;
        }
    }
    
    DS3231_LOG_W("Schedule %d not found", scheduleId);
    return false;
}

bool DS3231Controller::removeSchedule(uint8_t scheduleId) {
    auto it = std::remove_if(_schedules.begin(), _schedules.end(),
                            [scheduleId](const Schedule& s) { return s.id == scheduleId; });
    
    if (it != _schedules.end()) {
        _schedules.erase(it, _schedules.end());
        DS3231_LOG_I("Removed schedule %d", scheduleId);
        (void)setAlarmForNextSchedule();
        return true;
    }
    
    DS3231_LOG_W("Schedule %d not found", scheduleId);
    return false;
}

DS3231Controller::Schedule* DS3231Controller::getSchedule(uint8_t scheduleId) {
    for (auto& schedule : _schedules) {
        if (schedule.id == scheduleId) {
            return &schedule;
        }
    }
    return nullptr;
}

void DS3231Controller::clearAllSchedules() {
    _schedules.clear();
    DS3231_LOG_I("All schedules cleared");
}

bool DS3231Controller::isWithinAnySchedule() const {
    if (!_initialized) {
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }

    if (_vacationMode.enabled) {
        DateTime now = _rtc.now();
        if (now >= _vacationMode.startDate && now <= _vacationMode.endDate) {
            DS3231_LOG_D("Vacation mode active, schedules disabled");
            return false;
        }
    }

    for (const auto& schedule : _schedules) {
        if (isWithinSchedule(schedule.id)) {
            return true;
        }
    }

    return false;
}

bool DS3231Controller::isWithinSchedule(uint8_t scheduleId) const {
    if (!_initialized) {
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }

    const Schedule* schedule = nullptr;
    for (const auto& sched : _schedules) {
        if (sched.id == scheduleId) {
            schedule = &sched;
            break;
        }
    }

    if (!schedule || !schedule->enabled) {
        return false;
    }

    DateTime current = _rtc.now();

    // Check day of week
    if (!schedule->isDayEnabled(current.dayOfTheWeek())) {
        return false;
    }

    // Check time range
    return isTimeInRange(current, schedule->startHour, schedule->startMinute,
                        schedule->endHour, schedule->endMinute);
}

DS3231Controller::Schedule* DS3231Controller::getCurrentActiveSchedule() {
    for (auto& schedule : _schedules) {
        if (isWithinSchedule(schedule.id)) {
            return &schedule;
        }
    }
    return nullptr;
}

const DS3231Controller::Schedule* DS3231Controller::getCurrentActiveSchedule() const {
    for (const auto& schedule : _schedules) {
        if (isWithinSchedule(schedule.id)) {
            return &schedule;
        }
    }
    return nullptr;
}

DateTime DS3231Controller::getNextScheduledStart() const {
    if (!_initialized) {
        return DateTime();
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return DateTime();
    }

    DateTime now = _rtc.now();
    DateTime nextStart;
    bool found = false;
    
    for (const auto& schedule : _schedules) {
        if (!schedule.enabled) continue;
        
        DateTime scheduleNext = calculateNextOccurrence(schedule, now);
        if (scheduleNext.isValid() && (!found || scheduleNext < nextStart)) {
            nextStart = scheduleNext;
            found = true;
        }
    }
    
    return nextStart;
}

DateTime DS3231Controller::getNextScheduledEnd() const {
    if (!_initialized) {
        return DateTime();
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return DateTime();
    }

    DateTime now = _rtc.now();
    DateTime nextEnd;
    bool found = false;

    for (const auto& schedule : _schedules) {
        if (!schedule.enabled) continue;

        // Check if we're currently in this schedule
        if (isWithinSchedule(schedule.id)) {
            // We're in this schedule, so return its end time today
            DateTime endToday = DateTime(now.year(), now.month(), now.day(),
                                        schedule.endHour, schedule.endMinute, 0);

            // Handle schedules that span midnight
            if (schedule.endHour < schedule.startHour ||
                (schedule.endHour == schedule.startHour && schedule.endMinute < schedule.startMinute)) {
                endToday = endToday + TimeSpan(1, 0, 0, 0); // Add one day
            }

            if (!found || endToday < nextEnd) {
                nextEnd = endToday;
                found = true;
            }
        }
    }

    return nextEnd;
}

uint32_t DS3231Controller::getSecondsUntilNextEvent() const {
    if (!_initialized) {
        return 0xFFFFFFFF;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return 0xFFFFFFFF;
    }

    DateTime now = _rtc.now();
    DateTime nextStart = getNextScheduledStart();
    DateTime nextEnd = getNextScheduledEnd();

    uint32_t secondsToStart = 0xFFFFFFFF;
    uint32_t secondsToEnd = 0xFFFFFFFF;

    if (nextStart.isValid() && nextStart > now) {
        secondsToStart = nextStart.unixtime() - now.unixtime();
    }

    if (nextEnd.isValid() && nextEnd > now) {
        secondsToEnd = nextEnd.unixtime() - now.unixtime();
    }

    // Return the soonest event
    return (secondsToStart < secondsToEnd) ? secondsToStart : secondsToEnd;
}

bool DS3231Controller::isTimeInRange(const DateTime& current, uint8_t startHour, uint8_t startMinute,
                                     uint8_t endHour, uint8_t endMinute) const {
    uint16_t currentMinutes = current.hour() * 60 + current.minute();
    uint16_t startMinutes = startHour * 60 + startMinute;
    uint16_t endMinutes = endHour * 60 + endMinute;
    
    if (startMinutes <= endMinutes) {
        // Normal case: start and end on same day
        return currentMinutes >= startMinutes && currentMinutes < endMinutes;
    } else {
        // Spans midnight
        return currentMinutes >= startMinutes || currentMinutes < endMinutes;
    }
}

void DS3231Controller::setVacationMode(bool enabled, const DateTime& start, const DateTime& end) {
    _vacationMode.enabled = enabled;
    _vacationMode.startDate = start;
    _vacationMode.endDate = end;
    
    DS3231_LOG_I("Vacation mode %s", enabled ? "enabled" : "disabled");
    if (enabled) {
        DS3231_LOG_I("Vacation period: %s to %s", 
                     start.timestamp(DateTime::TIMESTAMP_DATE).c_str(),
                     end.timestamp(DateTime::TIMESTAMP_DATE).c_str());
    }
}

bool DS3231Controller::isVacationMode() const {
    if (!_vacationMode.enabled) return false;
    if (!_initialized) return false;

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }

    DateTime now = _rtc.now();
    return now >= _vacationMode.startDate && now <= _vacationMode.endDate;
}

void DS3231Controller::setPumpExercise(bool enabled, uint8_t dayOfMonth, uint8_t hour,
                                       uint8_t minute, uint16_t durationSeconds) {
    _pumpExercise.enabled = enabled;
    _pumpExercise.dayOfMonth = dayOfMonth;
    _pumpExercise.hour = hour;
    _pumpExercise.minute = minute;
    _pumpExercise.durationSeconds = durationSeconds;

    DS3231_LOG_I("Pump exercise %s: day %d at %02d:%02d for %d seconds",
                 enabled ? "enabled" : "disabled", dayOfMonth, hour, minute, durationSeconds);
}

bool DS3231Controller::isPumpExerciseTime() const {
    if (!_pumpExercise.enabled) return false;
    if (!_initialized) return false;

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }

    DateTime now = _rtc.now();

    // Check if we're in vacation mode but pump exercise is allowed
    if (isVacationMode() && !_vacationMode.runPumpExercise) {
        return false;
    }

    // Check day, hour, and minute
    if (now.day() == _pumpExercise.dayOfMonth &&
        now.hour() == _pumpExercise.hour &&
        now.minute() == _pumpExercise.minute) {
        
        // Check if we already ran it this month
        if (_pumpExercise.lastRun.isValid() &&
            _pumpExercise.lastRun.year() == now.year() &&
            _pumpExercise.lastRun.month() == now.month()) {
            return false;  // Already ran this month
        }
        
        return true;
    }
    
    return false;
}

void DS3231Controller::markPumpExerciseComplete() {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for markPumpExerciseComplete()");
        return;
    }

    _pumpExercise.lastRun = _rtc.now();
    DS3231_LOG_I("Pump exercise completed at %s",
                 _pumpExercise.lastRun.timestamp(DateTime::TIMESTAMP_FULL).c_str());
}

DS3231Controller::TemperatureData DS3231Controller::getTemperature() {
    TemperatureData data;
    data.celsius = 0.0f;
    data.fahrenheit = 32.0f;

    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return data;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for getTemperature()");
        return data;
    }

    data.celsius = _rtc.getTemperature();
    data.fahrenheit = data.celsius * 9.0 / 5.0 + 32.0;
    data.timestamp = _rtc.now();

    DS3231_LOG_D("Temperature: %.2f°C / %.2f°F", data.celsius, data.fahrenheit);

    return data;
}

float DS3231Controller::getTemperatureCelsius() {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return 0.0f;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for getTemperatureCelsius()");
        return 0.0f;
    }

    return _rtc.getTemperature();
}

bool DS3231Controller::setAlarmForNextSchedule() {
    DateTime next = getNextScheduledStart();
    if (!next.isValid()) {
        DS3231_LOG_W("No upcoming schedules to set alarm for");
        return false;
    }
    
    // Use Alarm 1 for schedule starts
    return setAlarm1(next, false);
}

bool DS3231Controller::setAlarm1(const DateTime& dt, bool matchSeconds) {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return false;
    }

    if (!dt.isValid()) {
        DS3231_LOG_E("Invalid DateTime for Alarm 1");
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for setAlarm1()");
        return false;
    }

    DS3231_LOG_I("Setting Alarm 1 for %s", dt.timestamp(DateTime::TIMESTAMP_FULL).c_str());

    if (matchSeconds) {
        _rtc.setAlarm1(dt, DS3231_A1_Date);
    } else {
        _rtc.setAlarm1(dt, DS3231_A1_Hour);
    }

    return true;
}

void DS3231Controller::clearAlarm(uint8_t alarmNumber) {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for clearAlarm()");
        return;
    }

    _rtc.clearAlarm(alarmNumber);
    DS3231_LOG_D("Cleared alarm %d", alarmNumber);
}

bool DS3231Controller::isAlarmFired(uint8_t alarmNumber) {
    if (!_initialized) {
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }

    return _rtc.alarmFired(alarmNumber);
}

void DS3231Controller::acknowledgeAlarm(uint8_t alarmNumber) {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for acknowledgeAlarm()");
        return;
    }

    _rtc.clearAlarm(alarmNumber);

    if (_alarmCallback) {
        _alarmCallback(alarmNumber);
    }

    DS3231_LOG_D("Acknowledged alarm %d", alarmNumber);
}

String DS3231Controller::getFormattedTime() const {
    if (!_initialized) {
        return "--:--:--";
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return "--:--:--";
    }

    DateTime now = _rtc.now();
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());
    return String(buffer);
}

String DS3231Controller::getFormattedDate() const {
    if (!_initialized) {
        return "----/--/--";
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return "----/--/--";
    }

    DateTime now = _rtc.now();
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
             now.year(), now.month(), now.day());
    return String(buffer);
}

String DS3231Controller::getScheduleStatus() const {
    const Schedule* active = getCurrentActiveSchedule();

    if (isVacationMode()) {
        return "Vacation Mode Active";
    }

    if (active) {
        return "Active: " + active->name;
    }

    DateTime next = getNextScheduledStart();
    if (next.isValid()) {
        return "Next: " + next.timestamp(DateTime::TIMESTAMP_TIME);
    }

    return "No Active Schedules";
}

void DS3231Controller::printDiagnostics() {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for printDiagnostics()");
        return;
    }

    DateTime now = _rtc.now();
    float temp = _rtc.getTemperature();

    DS3231_LOG_I("=== DS3231 Diagnostics ===");
    DS3231_LOG_I("Current Time: %s", now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
    DS3231_LOG_I("Temperature: %.2f°C", temp);
    DS3231_LOG_I("Total Schedules: %d", _schedules.size());

    for (const auto& schedule : _schedules) {
        DS3231_LOG_I("  Schedule %d '%s': %s, %02d:%02d-%02d:%02d, days=%s",
                     schedule.id, schedule.name.c_str(),
                     schedule.enabled ? "ON" : "OFF",
                     schedule.startHour, schedule.startMinute,
                     schedule.endHour, schedule.endMinute,
                     formatDayMask(schedule.dayMask).c_str());
    }

    DS3231_LOG_I("Vacation Mode: %s", _vacationMode.enabled ? "ON" : "OFF");
    DS3231_LOG_I("Pump Exercise: %s", _pumpExercise.enabled ? "ON" : "OFF");
    DS3231_LOG_I("Current Status: %s", getScheduleStatus().c_str());
    DS3231_LOG_I("==========================");
}

uint8_t DS3231Controller::getNextFreeScheduleId() const {
    uint8_t id = 1;
    bool found;
    
    do {
        found = false;
        for (const auto& schedule : _schedules) {
            if (schedule.id == id) {
                found = true;
                id++;
                break;
            }
        }
    } while (found && id < 255);
    
    return id;
}

DateTime DS3231Controller::calculateNextOccurrence(const Schedule& schedule, const DateTime& from) const {
    if (!schedule.enabled || schedule.dayMask == 0) {
        return DateTime();  // Invalid
    }
    
    DateTime next = from;
    
    // Start from next minute to avoid matching current time
    next = next + TimeSpan(0, 0, 1, 0);
    
    for (int days = 0; days < 8; days++) {  // Check up to a week ahead
        if (schedule.isDayEnabled(next.dayOfTheWeek())) {
            // Check if we can use this day
            DateTime candidate(next.year(), next.month(), next.day(),
                             schedule.startHour, schedule.startMinute, 0);
            
            if (candidate > from) {
                return candidate;
            }
        }
        
        // Move to next day
        next = next + TimeSpan(1, 0, 0, 0);
    }
    
    return DateTime();  // Should never reach here if schedule has valid days
}

String DS3231Controller::formatDayMask(uint8_t dayMask) {
    const char* days[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    String result;
    
    for (int i = 0; i < 7; i++) {
        if (dayMask & (1 << i)) {
            if (result.length() > 0) result += ",";
            result += days[i];
        }
    }
    
    return result.length() > 0 ? result : "None";
}

const char* DS3231Controller::dayOfWeekStr(uint8_t dow) {
    static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return (dow < 7) ? days[dow] : "???";
}

uint8_t DS3231Controller::dayOfWeekFromStr(const char* str) {
    // Support both short and long day names
    static const char* shortDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* longDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                     "Thursday", "Friday", "Saturday"};

    for (uint8_t i = 0; i < 7; i++) {
        if (strcasecmp(str, shortDays[i]) == 0 || strcasecmp(str, longDays[i]) == 0) {
            return i;
        }
    }

    return 255;  // Invalid
}

// Persistence methods
size_t DS3231Controller::getScheduleDataSize() const {
    // Header (4 bytes) + each schedule size
    size_t size = 4; // Magic number (2) + version (1) + schedule count (1)
    size += _schedules.size() * (sizeof(Schedule) - sizeof(String) + 32); // 32 chars max for name
    size += sizeof(VacationMode);
    size += sizeof(PumpExercise);
    return size;
}

bool DS3231Controller::serializeSchedules(uint8_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < getScheduleDataSize()) {
        DS3231_LOG_E("Invalid buffer or insufficient size");
        return false;
    }
    
    size_t offset = 0;
    
    // Write header
    buffer[offset++] = 0xD3; // Magic number
    buffer[offset++] = 0x23;
    buffer[offset++] = 1;    // Version
    buffer[offset++] = _schedules.size();
    
    // Write schedules
    for (const auto& schedule : _schedules) {
        buffer[offset++] = schedule.id;
        buffer[offset++] = schedule.dayMask;
        buffer[offset++] = schedule.startHour;
        buffer[offset++] = schedule.startMinute;
        buffer[offset++] = schedule.endHour;
        buffer[offset++] = schedule.endMinute;
        buffer[offset++] = schedule.enabled ? 1 : 0;
        
        // Write name (max 31 chars + null terminator)
        size_t nameLen = schedule.name.length();
        if (nameLen > 31) nameLen = 31;
        memcpy(&buffer[offset], schedule.name.c_str(), nameLen);
        buffer[offset + nameLen] = 0;
        offset += 32;
    }
    
    // Write vacation mode
    memcpy(&buffer[offset], &_vacationMode, sizeof(VacationMode));
    offset += sizeof(VacationMode);
    
    // Write pump exercise
    memcpy(&buffer[offset], &_pumpExercise, sizeof(PumpExercise));
    
    DS3231_LOG_I("Serialized %d schedules to buffer", _schedules.size());
    return true;
}

bool DS3231Controller::deserializeSchedules(const uint8_t* buffer, size_t dataSize) {
    if (!buffer || dataSize < 4) {
        DS3231_LOG_E("Invalid buffer or size");
        return false;
    }
    
    size_t offset = 0;
    
    // Verify header
    if (buffer[offset++] != 0xD3 || buffer[offset++] != 0x23) {
        DS3231_LOG_E("Invalid magic number");
        return false;
    }
    
    uint8_t version = buffer[offset++];
    if (version != 1) {
        DS3231_LOG_E("Unsupported version: %d", version);
        return false;
    }
    
    uint8_t scheduleCount = buffer[offset++];
    if (scheduleCount > MAX_SCHEDULES) {
        DS3231_LOG_E("Too many schedules: %d", scheduleCount);
        return false;
    }
    
    // Clear existing schedules
    _schedules.clear();
    
    // Read schedules
    for (uint8_t i = 0; i < scheduleCount; i++) {
        Schedule schedule;
        schedule.id = buffer[offset++];
        schedule.dayMask = buffer[offset++];
        schedule.startHour = buffer[offset++];
        schedule.startMinute = buffer[offset++];
        schedule.endHour = buffer[offset++];
        schedule.endMinute = buffer[offset++];
        schedule.enabled = buffer[offset++] != 0;
        
        // Read name
        char name[32];
        memcpy(name, &buffer[offset], 32);
        name[31] = 0; // Ensure null termination
        schedule.name = String(name);
        offset += 32;
        
        _schedules.push_back(schedule);
    }
    
    // Read vacation mode
    if (offset + sizeof(VacationMode) <= dataSize) {
        memcpy(&_vacationMode, &buffer[offset], sizeof(VacationMode));
        offset += sizeof(VacationMode);
    }
    
    // Read pump exercise
    if (offset + sizeof(PumpExercise) <= dataSize) {
        memcpy(&_pumpExercise, &buffer[offset], sizeof(PumpExercise));
    }
    
    DS3231_LOG_I("Deserialized %d schedules from buffer", scheduleCount);
    return true;
}

// Additional missing implementations

bool DS3231Controller::adjustDrift(int32_t secondsPerMonth) {
    // DS3231 doesn't have a direct drift adjustment register
    // This would require manual time adjustment over time
    DS3231_LOG_W("Drift adjustment not implemented for DS3231");
    return false;
}

bool DS3231Controller::isTemperatureCompensationEnabled() const {
    // DS3231 has built-in temperature compensation that's always enabled
    return true;
}

bool DS3231Controller::setAlarm2(const DateTime& dt) {
    if (!_initialized) {
        DS3231_LOG_E("RTC not initialized - call begin() first");
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for setAlarm2()");
        return false;
    }

    // Set alarm 2 (minute precision)
    _rtc.setAlarm2(dt, DS3231_A2_Minute);
    DS3231_LOG_I("Alarm 2 set for %02d:%02d", dt.hour(), dt.minute());
    return true;
}

void DS3231Controller::enableBatteryBackup(bool enable) {
    // DS3231 battery backup is hardware-based and always enabled when battery is present
    DS3231_LOG_W("Battery backup is hardware-controlled on DS3231");
}

bool DS3231Controller::isBatteryBackupEnabled() const {
    if (!_initialized) {
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        return false;
    }

    // Check if oscillator stop flag is set (indicates power loss)
    return !_rtc.lostPower();
}

float DS3231Controller::getBatteryVoltage() const {
    // DS3231 doesn't provide battery voltage monitoring
    DS3231_LOG_W("Battery voltage monitoring not available on DS3231");
    return -1.0f;
}

// Timezone-aware time management

bool DS3231Controller::setTimeFromUTC(uint32_t utcEpoch, int32_t offsetSeconds) {
    // Validate input
    if (utcEpoch < 946684800UL) { // Before year 2000
        DS3231_LOG_E("Invalid UTC epoch: %lu (before year 2000)", utcEpoch);
        return false;
    }
    
    // Convert UTC epoch to local time by adding offset
    uint32_t localEpoch = utcEpoch + offsetSeconds;
    DateTime localTime(localEpoch);
    
    DS3231_LOG_D("Setting RTC from UTC: UTC epoch=%lu, offset=%ld, local epoch=%lu", 
                 utcEpoch, offsetSeconds, localEpoch);
    DS3231_LOG_D("Local time will be: %s", localTime.timestamp(DateTime::TIMESTAMP_FULL).c_str());
    
    // Debug: Let's see what DateTime thinks the components are
    DS3231_LOG_D("DateTime components: %04d-%02d-%02d %02d:%02d:%02d",
                 localTime.year(), localTime.month(), localTime.day(),
                 localTime.hour(), localTime.minute(), localTime.second());
    
    // Extra validation
    if (localTime.year() < 2000 || localTime.year() > 2100) {
        DS3231_LOG_E("Invalid year %d after conversion - rejecting time update", localTime.year());
        return false;
    }
    
    return setTime(localTime);
}

uint32_t DS3231Controller::nowUTC(int32_t offsetSeconds) const {
    DateTime localTime = now();
    uint32_t localEpoch = localTime.unixtime();
    uint32_t utcEpoch = localEpoch - offsetSeconds;

    DS3231_LOG_D("Converting RTC to UTC: local epoch=%lu, offset=%ld, UTC epoch=%lu",
                 localEpoch, offsetSeconds, utcEpoch);

    return utcEpoch;
}

bool DS3231Controller::syncSystemTime() const {
    if (!_initialized) {
        DS3231_LOG_E("Cannot sync system time - RTC not initialized");
        return false;
    }

    RecursiveMutexGuard lock(_mutex);
    if (!lock.hasLock()) {
        DS3231_LOG_E("Failed to acquire mutex for syncSystemTime()");
        return false;
    }

    DateTime rtcTime = _rtc.now();
    if (!rtcTime.isValid()) {
        DS3231_LOG_E("Invalid RTC time - cannot sync system time");
        return false;
    }

    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(rtcTime.unixtime());
    tv.tv_usec = 0;  // RTC has 1-second resolution, no sub-second precision

    if (settimeofday(&tv, nullptr) != 0) {
        DS3231_LOG_E("settimeofday() failed");
        return false;
    }

    DS3231_LOG_I("System time synced from RTC: %s (note: sub-second precision is 0)",
                 rtcTime.timestamp(DateTime::TIMESTAMP_FULL).c_str());
    return true;
}