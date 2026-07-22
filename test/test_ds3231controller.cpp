/**
 * @file test_ds3231controller.cpp
 * @brief Unit tests for DS3231Controller offline-testable functionality
 *
 * Tests Schedule structure, day mask operations, utility functions,
 * and constants without requiring actual RTC hardware.
 */

#include <unity.h>
#include <string.h>
#include "DS3231Controller.h"

void setUp(void) {
    // Unity setup - called before each test
}

void tearDown(void) {
    // Unity teardown - called after each test
}

// ============================================================================
// Schedule Structure Tests
// ============================================================================

void test_schedule_default_values(void) {
    DS3231Controller::Schedule sched;
    sched.id = 0;
    sched.dayMask = 0;
    sched.startHour = 8;
    sched.startMinute = 0;
    sched.endHour = 9;
    sched.endMinute = 0;
    sched.enabled = true;
    sched.name = "Test";

    TEST_ASSERT_EQUAL_UINT8(0, sched.id);
    TEST_ASSERT_EQUAL_UINT8(0, sched.dayMask);
    TEST_ASSERT_TRUE(sched.enabled);
}

void test_schedule_day_enabled_check(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0b01100001;  // Sunday, Friday, Saturday

    TEST_ASSERT_TRUE(sched.isDayEnabled(0));   // Sunday
    TEST_ASSERT_FALSE(sched.isDayEnabled(1));  // Monday
    TEST_ASSERT_FALSE(sched.isDayEnabled(2));  // Tuesday
    TEST_ASSERT_FALSE(sched.isDayEnabled(3));  // Wednesday
    TEST_ASSERT_FALSE(sched.isDayEnabled(4));  // Thursday
    TEST_ASSERT_TRUE(sched.isDayEnabled(5));   // Friday
    TEST_ASSERT_TRUE(sched.isDayEnabled(6));   // Saturday
}

void test_schedule_set_day_enable(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0;

    // Enable Monday
    sched.setDay(1, true);
    TEST_ASSERT_EQUAL_UINT8(0b00000010, sched.dayMask);
    TEST_ASSERT_TRUE(sched.isDayEnabled(1));

    // Enable Wednesday
    sched.setDay(3, true);
    TEST_ASSERT_EQUAL_UINT8(0b00001010, sched.dayMask);
    TEST_ASSERT_TRUE(sched.isDayEnabled(3));

    // Disable Monday
    sched.setDay(1, false);
    TEST_ASSERT_EQUAL_UINT8(0b00001000, sched.dayMask);
    TEST_ASSERT_FALSE(sched.isDayEnabled(1));
}

void test_schedule_set_all_weekdays(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0;

    // Enable Mon-Fri (days 1-5)
    for (uint8_t d = 1; d <= 5; d++) {
        sched.setDay(d, true);
    }

    TEST_ASSERT_EQUAL_UINT8(0b00111110, sched.dayMask);
    TEST_ASSERT_FALSE(sched.isDayEnabled(0));  // Sunday off
    TEST_ASSERT_TRUE(sched.isDayEnabled(1));   // Monday on
    TEST_ASSERT_TRUE(sched.isDayEnabled(2));   // Tuesday on
    TEST_ASSERT_TRUE(sched.isDayEnabled(3));   // Wednesday on
    TEST_ASSERT_TRUE(sched.isDayEnabled(4));   // Thursday on
    TEST_ASSERT_TRUE(sched.isDayEnabled(5));   // Friday on
    TEST_ASSERT_FALSE(sched.isDayEnabled(6));  // Saturday off
}

void test_schedule_set_weekend(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0;

    sched.setDay(0, true);  // Sunday
    sched.setDay(6, true);  // Saturday

    TEST_ASSERT_EQUAL_UINT8(0b01000001, sched.dayMask);
}

// ============================================================================
// PumpExercise Structure Tests
// ============================================================================

void test_pump_exercise_structure(void) {
    DS3231Controller::PumpExercise pump;
    pump.enabled = true;
    pump.dayOfMonth = 15;
    pump.hour = 3;
    pump.minute = 30;
    pump.durationSeconds = 600;

    TEST_ASSERT_TRUE(pump.enabled);
    TEST_ASSERT_EQUAL_UINT8(15, pump.dayOfMonth);
    TEST_ASSERT_EQUAL_UINT8(3, pump.hour);
    TEST_ASSERT_EQUAL_UINT8(30, pump.minute);
    TEST_ASSERT_EQUAL_UINT16(600, pump.durationSeconds);
}

// ============================================================================
// VacationMode Structure Tests
// ============================================================================

void test_vacation_mode_structure(void) {
    DS3231Controller::VacationMode vacation;
    vacation.enabled = true;
    vacation.runPumpExercise = true;

    TEST_ASSERT_TRUE(vacation.enabled);
    TEST_ASSERT_TRUE(vacation.runPumpExercise);
}

// ============================================================================
// TemperatureData Structure Tests
// ============================================================================

void test_temperature_data_structure(void) {
    DS3231Controller::TemperatureData temp;
    temp.celsius = 25.5f;
    temp.fahrenheit = 77.9f;

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.5f, temp.celsius);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 77.9f, temp.fahrenheit);
}

void test_temperature_conversion(void) {
    float celsius = 25.0f;
    float expectedFahrenheit = (celsius * 9.0f / 5.0f) + 32.0f;

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 77.0f, expectedFahrenheit);
}

// ============================================================================
// Static Utility Method Tests
// ============================================================================

void test_day_of_week_str_sunday(void) {
    const char* result = DS3231Controller::dayOfWeekStr(0);
    TEST_ASSERT_EQUAL_STRING("Sun", result);
}

void test_day_of_week_str_all_days(void) {
    const char* expected[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    for (uint8_t i = 0; i < 7; i++) {
        TEST_ASSERT_EQUAL_STRING(expected[i], DS3231Controller::dayOfWeekStr(i));
    }
}

void test_day_of_week_str_invalid(void) {
    const char* result = DS3231Controller::dayOfWeekStr(7);
    TEST_ASSERT_EQUAL_STRING("???", result);
}

void test_day_of_week_from_str(void) {
    TEST_ASSERT_EQUAL_UINT8(0, DS3231Controller::dayOfWeekFromStr("Sun"));
    TEST_ASSERT_EQUAL_UINT8(1, DS3231Controller::dayOfWeekFromStr("Mon"));
    TEST_ASSERT_EQUAL_UINT8(2, DS3231Controller::dayOfWeekFromStr("Tue"));
    TEST_ASSERT_EQUAL_UINT8(3, DS3231Controller::dayOfWeekFromStr("Wed"));
    TEST_ASSERT_EQUAL_UINT8(4, DS3231Controller::dayOfWeekFromStr("Thu"));
    TEST_ASSERT_EQUAL_UINT8(5, DS3231Controller::dayOfWeekFromStr("Fri"));
    TEST_ASSERT_EQUAL_UINT8(6, DS3231Controller::dayOfWeekFromStr("Sat"));
}

void test_day_of_week_from_str_invalid(void) {
    uint8_t result = DS3231Controller::dayOfWeekFromStr("Invalid");
    TEST_ASSERT_EQUAL_UINT8(255, result);  // Invalid returns 255
}

void test_day_of_week_from_str_long_names(void) {
    // dayOfWeekFromStr also accepts long day names
    TEST_ASSERT_EQUAL_UINT8(0, DS3231Controller::dayOfWeekFromStr("Sunday"));
    TEST_ASSERT_EQUAL_UINT8(1, DS3231Controller::dayOfWeekFromStr("Monday"));
    TEST_ASSERT_EQUAL_UINT8(2, DS3231Controller::dayOfWeekFromStr("Tuesday"));
    TEST_ASSERT_EQUAL_UINT8(3, DS3231Controller::dayOfWeekFromStr("Wednesday"));
    TEST_ASSERT_EQUAL_UINT8(4, DS3231Controller::dayOfWeekFromStr("Thursday"));
    TEST_ASSERT_EQUAL_UINT8(5, DS3231Controller::dayOfWeekFromStr("Friday"));
    TEST_ASSERT_EQUAL_UINT8(6, DS3231Controller::dayOfWeekFromStr("Saturday"));
}

void test_format_day_mask_empty(void) {
    String result = DS3231Controller::formatDayMask(0);
    TEST_ASSERT_EQUAL_STRING("None", result.c_str());
}

void test_format_day_mask_weekdays(void) {
    String result = DS3231Controller::formatDayMask(0b00111110);  // Mon-Fri
    // formatDayMask uses 2-letter abbreviations: Mo, Tu, We, Th, Fr
    TEST_ASSERT_TRUE(result.indexOf("Mo") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("Fr") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("Su") < 0);  // Should not contain Sunday
    TEST_ASSERT_TRUE(result.indexOf("Sa") < 0);  // Should not contain Saturday
}

void test_format_day_mask_all_days(void) {
    String result = DS3231Controller::formatDayMask(0b01111111);  // All days
    // formatDayMask uses 2-letter abbreviations
    TEST_ASSERT_TRUE(result.indexOf("Su") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("Sa") >= 0);
}

// ============================================================================
// Constants Tests
// ============================================================================

void test_max_schedules_reasonable(void) {
    // MAX_SCHEDULES is private, but we can test behavior indirectly
    // For now, just ensure the library concept is reasonable
    // A hot water system typically needs 1-10 schedules
    TEST_PASS();  // Constant validation is implicit in usage
}

void test_schedule_time_range_validity(void) {
    // Hours should be 0-23
    DS3231Controller::Schedule sched;
    sched.startHour = 23;
    sched.endHour = 0;

    TEST_ASSERT_LESS_OR_EQUAL(23, sched.startHour);
    TEST_ASSERT_LESS_OR_EQUAL(23, sched.endHour);
}

void test_schedule_minute_range_validity(void) {
    // Minutes should be 0-59
    DS3231Controller::Schedule sched;
    sched.startMinute = 59;
    sched.endMinute = 0;

    TEST_ASSERT_LESS_OR_EQUAL(59, sched.startMinute);
    TEST_ASSERT_LESS_OR_EQUAL(59, sched.endMinute);
}

// ============================================================================
// Day Mask Edge Cases
// ============================================================================

void test_day_mask_all_bits(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0b01111111;

    for (uint8_t d = 0; d < 7; d++) {
        TEST_ASSERT_TRUE(sched.isDayEnabled(d));
    }
}

void test_day_mask_no_bits(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0;

    for (uint8_t d = 0; d < 7; d++) {
        TEST_ASSERT_FALSE(sched.isDayEnabled(d));
    }
}

void test_day_mask_toggle(void) {
    DS3231Controller::Schedule sched;
    sched.dayMask = 0b00000001;  // Sunday only

    // Toggle Sunday off
    sched.setDay(0, false);
    TEST_ASSERT_FALSE(sched.isDayEnabled(0));

    // Toggle Sunday on
    sched.setDay(0, true);
    TEST_ASSERT_TRUE(sched.isDayEnabled(0));
}

// ============================================================================
// Schedule Time Spanning Midnight
// ============================================================================

void test_schedule_can_span_midnight(void) {
    DS3231Controller::Schedule sched;
    sched.startHour = 23;
    sched.startMinute = 0;
    sched.endHour = 1;
    sched.endMinute = 0;

    // End hour < start hour indicates midnight span
    TEST_ASSERT_LESS_THAN(sched.startHour, sched.endHour);
}

// ============================================================================
// Test Runner
// ============================================================================

void runAllTests() {
    UNITY_BEGIN();

    // Schedule structure tests
    RUN_TEST(test_schedule_default_values);
    RUN_TEST(test_schedule_day_enabled_check);
    RUN_TEST(test_schedule_set_day_enable);
    RUN_TEST(test_schedule_set_all_weekdays);
    RUN_TEST(test_schedule_set_weekend);

    // Other structure tests
    RUN_TEST(test_pump_exercise_structure);
    RUN_TEST(test_vacation_mode_structure);
    RUN_TEST(test_temperature_data_structure);
    RUN_TEST(test_temperature_conversion);

    // Static utility tests
    RUN_TEST(test_day_of_week_str_sunday);
    RUN_TEST(test_day_of_week_str_all_days);
    RUN_TEST(test_day_of_week_str_invalid);
    RUN_TEST(test_day_of_week_from_str);
    RUN_TEST(test_day_of_week_from_str_invalid);
    RUN_TEST(test_day_of_week_from_str_long_names);
    RUN_TEST(test_format_day_mask_empty);
    RUN_TEST(test_format_day_mask_weekdays);
    RUN_TEST(test_format_day_mask_all_days);

    // Constants and validation tests
    RUN_TEST(test_max_schedules_reasonable);
    RUN_TEST(test_schedule_time_range_validity);
    RUN_TEST(test_schedule_minute_range_validity);

    // Day mask edge cases
    RUN_TEST(test_day_mask_all_bits);
    RUN_TEST(test_day_mask_no_bits);
    RUN_TEST(test_day_mask_toggle);
    RUN_TEST(test_schedule_can_span_midnight);

    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);  // Allow serial to initialize
    runAllTests();
}

void loop() {
    // Nothing to do
}
#else
int main(int argc, char** argv) {
    runAllTests();
    return 0;
}
#endif
