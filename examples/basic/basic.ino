/**
 * DS3231Controller Basic Example
 * 
 * This example demonstrates basic usage of the DS3231Controller library:
 * - Initialize the RTC
 * - Set and read time
 * - Create a simple schedule
 * - Monitor temperature
 * 
 * Connections:
 * - DS3231 SDA -> ESP32 GPIO 21
 * - DS3231 SCL -> ESP32 GPIO 22
 * - DS3231 VCC -> 3.3V
 * - DS3231 GND -> GND
 */

#include <Wire.h>
#include <DS3231Controller.h>

DS3231Controller rtc;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== DS3231Controller Basic Example ===");
    
    // Initialize I2C
    Wire.begin();
    
    // Initialize RTC
    if (!rtc.begin()) {
        Serial.println("Failed to initialize DS3231!");
        while (1) delay(1000);
    }
    
    Serial.println("DS3231 initialized successfully");
    
    // Print current time
    DateTime now = rtc.now();
    Serial.print("Current time: ");
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
    
    // Get temperature
    float temp = rtc.getTemperatureCelsius();
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println("°C");
    
    // Create a simple daily schedule (9:00 AM - 5:00 PM every day)
    DS3231Controller::Schedule dailySchedule;
    dailySchedule.name = "Daily Schedule";
    dailySchedule.startHour = 9;
    dailySchedule.startMinute = 0;
    dailySchedule.endHour = 17;
    dailySchedule.endMinute = 0;
    dailySchedule.dayMask = 0b11111111;  // All days
    dailySchedule.enabled = true;
    
    if (rtc.addSchedule(dailySchedule)) {
        Serial.println("Schedule added successfully");
    }
    
    // Set alarm for next schedule
    rtc.setAlarmForNextSchedule();
    
    Serial.println("\nSetup complete. Enter 's' for status, 't' for time.");
}

void loop() {
    static unsigned long lastCheck = 0;
    
    // Check status every 10 seconds
    if (millis() - lastCheck > 10000) {
        lastCheck = millis();
        
        // Check if we're in scheduled time
        if (rtc.isWithinAnySchedule()) {
            Serial.println("Currently within scheduled time");
        } else {
            DateTime next = rtc.getNextScheduledStart();
            if (next.isValid()) {
                Serial.print("Next schedule starts at: ");
                Serial.println(next.timestamp(DateTime::TIMESTAMP_TIME));
            }
        }
    }
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();  // Clear buffer
        
        switch (cmd) {
            case 's': {
                // Print status
                Serial.println("\n--- Status ---");
                Serial.print("Current time: ");
                Serial.println(rtc.getFormattedDate() + " " + rtc.getFormattedTime());
                Serial.print("Schedule status: ");
                Serial.println(rtc.getScheduleStatus());
                Serial.print("Temperature: ");
                Serial.print(rtc.getTemperatureCelsius());
                Serial.println("°C");
                break;
            }
            
            case 't': {
                // Set time (example: set to compile time)
                DateTime newTime(F(__DATE__), F(__TIME__));
                if (rtc.setTime(newTime)) {
                    Serial.print("Time set to: ");
                    Serial.println(newTime.timestamp(DateTime::TIMESTAMP_FULL));
                }
                break;
            }
            
            default:
                Serial.println("Commands: s=status, t=set time");
        }
    }
    
    delay(100);
}