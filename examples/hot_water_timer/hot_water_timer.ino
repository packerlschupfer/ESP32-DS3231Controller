/**
 * DS3231Controller Hot Water Timer Example
 * 
 * This example demonstrates using the DS3231Controller for a hot water system
 * with multiple daily schedules, vacation mode, and pump exercise features.
 * 
 * Hardware Requirements:
 * - ESP32 board
 * - DS3231 RTC module connected via I2C
 * - Relay module for controlling water heater/pump
 * 
 * Connections:
 * - DS3231 SDA -> ESP32 GPIO 21 (default I2C SDA)
 * - DS3231 SCL -> ESP32 GPIO 22 (default I2C SCL)
 * - DS3231 VCC -> 3.3V
 * - DS3231 GND -> GND
 * - Relay IN -> ESP32 GPIO 25
 */

#include <Wire.h>
#include <DS3231Controller.h>

// Pin definitions
#define RELAY_PIN 25          // Water heater/pump relay
#define STATUS_LED_PIN 2      // Built-in LED for status

// Create controller instance
DS3231Controller rtc;

// State tracking
bool heaterState = false;
unsigned long lastStatusPrint = 0;
const unsigned long STATUS_INTERVAL = 30000;  // Print status every 30 seconds

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== DS3231 Hot Water Timer Example ===");
    
    // Initialize pins
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // Ensure heater is off initially
    
    // Initialize I2C and RTC
    Wire.begin();
    
    if (!rtc.begin()) {
        Serial.println("ERROR: Failed to initialize DS3231!");
        while (1) {
            digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
            delay(500);
        }
    }
    
    // Set time if needed (uncomment and modify as needed)
    // rtc.setTime(DateTime(2024, 1, 15, 6, 30, 0));  // YYYY, MM, DD, HH, MM, SS
    
    // Configure schedules for hot water
    setupSchedules();
    
    // Set up pump exercise (run on 1st of each month at 3:00 AM for 5 minutes)
    rtc.setPumpExercise(true, 1, 3, 0, 300);
    
    // Set up callbacks
    rtc.onScheduleEvent([](const DS3231Controller::Schedule& schedule, bool isStart) {
        Serial.printf("Schedule Event: %s %s\n", 
                     schedule.name.c_str(), 
                     isStart ? "STARTED" : "ENDED");
        
        if (isStart) {
            activateHeater();
        } else {
            deactivateHeater();
        }
    });
    
    rtc.onAlarm([](uint8_t alarmNumber) {
        Serial.printf("Alarm %d fired!\n", alarmNumber);
    });
    
    // Print initial diagnostics
    rtc.printDiagnostics();
    
    Serial.println("\nSetup complete. Hot water timer is running.");
    Serial.println("Commands:");
    Serial.println("  s - Show status");
    Serial.println("  d - Show diagnostics");
    Serial.println("  v - Toggle vacation mode");
    Serial.println("  t - Show temperature");
    Serial.println("  1-7 - Toggle schedule");
    Serial.println();
}

void loop() {
    // Check if we're within any active schedule
    bool shouldBeOn = rtc.isWithinAnySchedule();
    
    // Check for pump exercise
    if (rtc.isPumpExerciseTime()) {
        Serial.println("PUMP EXERCISE: Starting monthly pump exercise");
        activateHeater();
        delay(rtc.getPumpExercise().durationSeconds * 1000);
        deactivateHeater();
        rtc.markPumpExerciseComplete();
    }
    
    // Update heater state if needed
    if (shouldBeOn != heaterState) {
        if (shouldBeOn) {
            activateHeater();
        } else {
            deactivateHeater();
        }
    }
    
    // Check for alarms
    if (rtc.isAlarmFired(1)) {
        rtc.acknowledgeAlarm(1);
        rtc.setAlarmForNextSchedule();  // Set next alarm
    }
    
    // Periodic status update
    if (millis() - lastStatusPrint > STATUS_INTERVAL) {
        printStatus();
        lastStatusPrint = millis();
    }
    
    // Handle serial commands
    handleSerialCommands();
    
    // Small delay to prevent excessive polling
    delay(100);
}

void setupSchedules() {
    // Weekday morning schedule (6:00 AM - 8:00 AM)
    DS3231Controller::Schedule morningWeekday;
    morningWeekday.name = "Weekday Morning";
    morningWeekday.startHour = 6;
    morningWeekday.startMinute = 0;
    morningWeekday.endHour = 8;
    morningWeekday.endMinute = 0;
    morningWeekday.enabled = true;
    // Monday through Friday
    morningWeekday.dayMask = 0b01111110;  // Bits: Sat Fri Thu Wed Tue Mon Sun
    rtc.addSchedule(morningWeekday);
    
    // Weekday evening schedule (6:00 PM - 9:00 PM)
    DS3231Controller::Schedule eveningWeekday;
    eveningWeekday.name = "Weekday Evening";
    eveningWeekday.startHour = 18;
    eveningWeekday.startMinute = 0;
    eveningWeekday.endHour = 21;
    eveningWeekday.endMinute = 0;
    eveningWeekday.enabled = true;
    eveningWeekday.dayMask = 0b01111110;
    rtc.addSchedule(eveningWeekday);
    
    // Weekend morning schedule (7:00 AM - 10:00 AM)
    DS3231Controller::Schedule morningWeekend;
    morningWeekend.name = "Weekend Morning";
    morningWeekend.startHour = 7;
    morningWeekend.startMinute = 0;
    morningWeekend.endHour = 10;
    morningWeekend.endMinute = 0;
    morningWeekend.enabled = true;
    // Saturday and Sunday
    morningWeekend.dayMask = 0b10000001;
    rtc.addSchedule(morningWeekend);
    
    // Weekend evening schedule (5:00 PM - 10:00 PM)
    DS3231Controller::Schedule eveningWeekend;
    eveningWeekend.name = "Weekend Evening";
    eveningWeekend.startHour = 17;
    eveningWeekend.startMinute = 0;
    eveningWeekend.endHour = 22;
    eveningWeekend.endMinute = 0;
    eveningWeekend.enabled = true;
    eveningWeekend.dayMask = 0b10000001;
    rtc.addSchedule(eveningWeekend);
}

void activateHeater() {
    if (!heaterState) {
        heaterState = true;
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(STATUS_LED_PIN, HIGH);
        Serial.println(">>> HEATER ACTIVATED");
        
        // Log temperature when starting
        auto temp = rtc.getTemperature();
        Serial.printf("Ambient temperature: %.1f°C / %.1f°F\n", 
                     temp.celsius, temp.fahrenheit);
    }
}

void deactivateHeater() {
    if (heaterState) {
        heaterState = false;
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(STATUS_LED_PIN, LOW);
        Serial.println("<<< HEATER DEACTIVATED");
    }
}

void printStatus() {
    Serial.println("\n--- Current Status ---");
    Serial.printf("Time: %s %s\n", 
                 rtc.getFormattedDate().c_str(), 
                 rtc.getFormattedTime().c_str());
    Serial.printf("Heater: %s\n", heaterState ? "ON" : "OFF");
    Serial.printf("Status: %s\n", rtc.getScheduleStatus().c_str());
    
    if (rtc.isVacationMode()) {
        auto vacation = rtc.getVacationMode();
        Serial.printf("VACATION MODE until %s\n", 
                     vacation.endDate.timestamp(DateTime::TIMESTAMP_DATE).c_str());
    }
    
    DateTime next = rtc.getNextScheduledStart();
    if (next.isValid()) {
        uint32_t seconds = rtc.getSecondsUntilNextEvent();
        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        Serial.printf("Next event in: %d hours %d minutes\n", hours, minutes);
    }
}

void handleSerialCommands() {
    if (!Serial.available()) return;
    
    char cmd = Serial.read();
    // Clear any remaining characters
    while (Serial.available()) Serial.read();
    
    switch (cmd) {
        case 's':
            printStatus();
            break;
            
        case 'd':
            rtc.printDiagnostics();
            break;
            
        case 'v': {
            bool vacationOn = !rtc.isVacationMode();
            if (vacationOn) {
                // Set vacation for next 7 days
                DateTime now = rtc.now();
                DateTime endDate = now + TimeSpan(7, 0, 0, 0);
                rtc.setVacationMode(true, now, endDate);
                Serial.println("Vacation mode ENABLED for 7 days");
            } else {
                rtc.setVacationMode(false);
                Serial.println("Vacation mode DISABLED");
            }
            break;
        }
        
        case 't': {
            auto temp = rtc.getTemperature();
            Serial.printf("RTC Temperature: %.2f°C / %.2f°F\n", 
                         temp.celsius, temp.fahrenheit);
            break;
        }
        
        case '1'...'7': {
            uint8_t scheduleId = cmd - '0';
            auto* schedule = rtc.getSchedule(scheduleId);
            if (schedule) {
                schedule->enabled = !schedule->enabled;
                rtc.updateSchedule(scheduleId, *schedule);
                Serial.printf("Schedule %d '%s' is now %s\n",
                            scheduleId, schedule->name.c_str(),
                            schedule->enabled ? "ENABLED" : "DISABLED");
            } else {
                Serial.printf("Schedule %d not found\n", scheduleId);
            }
            break;
        }
        
        default:
            Serial.println("Unknown command. Available commands:");
            Serial.println("  s - Show status");
            Serial.println("  d - Show diagnostics");
            Serial.println("  v - Toggle vacation mode");
            Serial.println("  t - Show temperature");
            Serial.println("  1-7 - Toggle schedule");
            break;
    }
}