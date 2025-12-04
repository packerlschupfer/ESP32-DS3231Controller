#ifndef DS3231_CONTROLLER_H
#define DS3231_CONTROLLER_H

#include <Arduino.h>
#include <RTClib.h>
#include <vector>
#include <functional>
#include <RecursiveMutexGuard.h>
#include "DS3231ControllerLogging.h"

class DS3231Controller {
public:
    // Schedule structure for hot water timing
    struct Schedule {
        uint8_t id;              // Unique schedule ID
        uint8_t dayMask;         // Bit mask for days (bit 0 = Sunday, bit 6 = Saturday)
        uint8_t startHour;       // 0-23
        uint8_t startMinute;     // 0-59
        uint8_t endHour;         // 0-23  
        uint8_t endMinute;       // 0-59
        bool enabled;            // Schedule active flag
        String name;             // e.g., "Morning Shower", "Evening Bath"
        
        // Helper methods
        bool isDayEnabled(uint8_t dayOfWeek) const {
            return (dayMask & (1 << dayOfWeek)) != 0;
        }
        
        void setDay(uint8_t dayOfWeek, bool enable) {
            if (enable) {
                dayMask |= (1 << dayOfWeek);
            } else {
                dayMask &= ~(1 << dayOfWeek);
            }
        }
    };

    // Pump exercise feature to prevent seizing
    struct PumpExercise {
        bool enabled;
        uint8_t dayOfMonth;      // 1-31 (0 = disabled)
        uint8_t hour;            // 0-23
        uint8_t minute;          // 0-59
        uint16_t durationSeconds; // How long to run
        DateTime lastRun;        // Track last execution
    };

    // Vacation mode settings
    struct VacationMode {
        bool enabled;
        DateTime startDate;
        DateTime endDate;
        bool runPumpExercise;    // Still run pump exercise during vacation
    };

    // Temperature data from DS3231
    struct TemperatureData {
        float celsius;
        float fahrenheit;
        DateTime timestamp;
    };

    // Callbacks
    using TimeChangeCallback = std::function<void(const DateTime&)>;
    using AlarmCallback = std::function<void(uint8_t alarmNumber)>;
    using ScheduleCallback = std::function<void(const Schedule&, bool isStart)>;

    // Constructor/Destructor
    DS3231Controller();
    ~DS3231Controller();

    // Initialization
    [[nodiscard]] bool begin(TwoWire* wire = &Wire);
    [[nodiscard]] bool isRunning() const;

    // Time management
    [[nodiscard]] bool setTime(const DateTime& dt);
    [[nodiscard]] DateTime now() const;
    [[nodiscard]] bool adjustDrift(int32_t secondsPerMonth);

    // Timezone-aware time management
    [[nodiscard]] bool setTimeFromUTC(uint32_t utcEpoch, int32_t offsetSeconds = 0);
    [[nodiscard]] uint32_t nowUTC(int32_t offsetSeconds = 0) const;

    // System time synchronization (RTC -> ESP32 system clock)
    [[nodiscard]] bool syncSystemTime() const;

    // Schedule management
    [[nodiscard]] bool addSchedule(const Schedule& schedule);
    [[nodiscard]] bool updateSchedule(uint8_t scheduleId, const Schedule& schedule);
    [[nodiscard]] bool removeSchedule(uint8_t scheduleId);
    [[nodiscard]] Schedule* getSchedule(uint8_t scheduleId);
    [[nodiscard]] const std::vector<Schedule>& getAllSchedules() const noexcept { return _schedules; }
    void clearAllSchedules();

    // Schedule queries
    [[nodiscard]] bool isWithinAnySchedule() const;
    [[nodiscard]] bool isWithinSchedule(uint8_t scheduleId) const;
    [[nodiscard]] Schedule* getCurrentActiveSchedule();
    [[nodiscard]] const Schedule* getCurrentActiveSchedule() const;
    [[nodiscard]] DateTime getNextScheduledStart() const;
    [[nodiscard]] DateTime getNextScheduledEnd() const;
    [[nodiscard]] uint32_t getSecondsUntilNextEvent() const;

    // Vacation mode
    void setVacationMode(bool enabled, const DateTime& start = DateTime(), const DateTime& end = DateTime());
    [[nodiscard]] bool isVacationMode() const;
    [[nodiscard]] VacationMode getVacationMode() const noexcept { return _vacationMode; }

    // Pump exercise (prevent pump seizing during summer)
    void setPumpExercise(bool enabled, uint8_t dayOfMonth = 1, uint8_t hour = 3,
                        uint8_t minute = 0, uint16_t durationSeconds = 300);
    [[nodiscard]] bool isPumpExerciseTime() const;
    [[nodiscard]] PumpExercise getPumpExercise() const noexcept { return _pumpExercise; }
    void markPumpExerciseComplete();

    // Temperature monitoring
    [[nodiscard]] TemperatureData getTemperature();
    [[nodiscard]] float getTemperatureCelsius();
    [[nodiscard]] bool isTemperatureCompensationEnabled() const;

    // Alarm management (DS3231 has 2 alarms)
    [[nodiscard]] bool setAlarmForNextSchedule();
    [[nodiscard]] bool setAlarm1(const DateTime& dt, bool matchSeconds = false);
    [[nodiscard]] bool setAlarm2(const DateTime& dt);
    void clearAlarm(uint8_t alarmNumber);
    [[nodiscard]] bool isAlarmFired(uint8_t alarmNumber);
    void acknowledgeAlarm(uint8_t alarmNumber);

    // Power management
    void enableBatteryBackup(bool enable);
    [[nodiscard]] bool isBatteryBackupEnabled() const;
    [[nodiscard]] float getBatteryVoltage() const;

    // Callbacks
    void onTimeChange(TimeChangeCallback callback) { _timeChangeCallback = callback; }
    void onAlarm(AlarmCallback callback) { _alarmCallback = callback; }
    void onScheduleEvent(ScheduleCallback callback) { _scheduleCallback = callback; }

    // Utility methods
    [[nodiscard]] String getFormattedTime() const;
    [[nodiscard]] String getFormattedDate() const;
    [[nodiscard]] String getScheduleStatus() const;
    void printDiagnostics();

    // Persistence (save/load schedules)
    [[nodiscard]] size_t getScheduleDataSize() const;
    [[nodiscard]] bool serializeSchedules(uint8_t* buffer, size_t bufferSize);
    [[nodiscard]] bool deserializeSchedules(const uint8_t* buffer, size_t dataSize);
    
    // Static utility methods
    static const char* dayOfWeekStr(uint8_t dow);
    static uint8_t dayOfWeekFromStr(const char* str);
    static String formatDayMask(uint8_t dayMask);

private:
    mutable RTC_DS3231 _rtc;
    std::vector<Schedule> _schedules;
    VacationMode _vacationMode;
    PumpExercise _pumpExercise;
    DateTime _lastCheck;
    uint8_t _activeScheduleId;
    bool _initialized = false;  // Prevent double initialization
    mutable SemaphoreHandle_t _mutex;  // Thread safety for I2C operations
    
    // Callbacks
    TimeChangeCallback _timeChangeCallback;
    AlarmCallback _alarmCallback;
    ScheduleCallback _scheduleCallback;
    
    // Internal methods
    bool isTimeInRange(const DateTime& current, uint8_t startHour, uint8_t startMinute,
                      uint8_t endHour, uint8_t endMinute) const;
    uint8_t getNextFreeScheduleId() const;
    void checkScheduleTransitions();
    void checkAlarms();
    DateTime calculateNextOccurrence(const Schedule& schedule, const DateTime& from) const;
    
    // Constants
    static constexpr uint8_t MAX_SCHEDULES = 10;
    static constexpr uint8_t SCHEDULE_CHECK_INTERVAL_SECONDS = 30;
    static constexpr uint8_t ALARM_1 = 1;
    static constexpr uint8_t ALARM_2 = 2;
};

#endif // DS3231_CONTROLLER_H