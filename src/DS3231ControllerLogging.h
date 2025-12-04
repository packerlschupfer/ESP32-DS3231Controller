#ifndef DS3231CONTROLLER_LOGGING_H
#define DS3231CONTROLLER_LOGGING_H

#define DS3231_LOG_TAG "DS3231"

#include <esp_log.h>  // Required for ESP_LOG_* constants

// Define log levels based on debug flag
#ifdef DS3231_DEBUG
    // Debug mode: Show all levels
    #define DS3231_LOG_LEVEL_E ESP_LOG_ERROR
    #define DS3231_LOG_LEVEL_W ESP_LOG_WARN
    #define DS3231_LOG_LEVEL_I ESP_LOG_INFO
    #define DS3231_LOG_LEVEL_D ESP_LOG_DEBUG
    #define DS3231_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define DS3231_LOG_LEVEL_E ESP_LOG_ERROR
    #define DS3231_LOG_LEVEL_W ESP_LOG_WARN
    #define DS3231_LOG_LEVEL_I ESP_LOG_INFO
    #define DS3231_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define DS3231_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include <LogInterface.h>
    #define DS3231_LOG_E(...) LOG_WRITE(DS3231_LOG_LEVEL_E, DS3231_LOG_TAG, __VA_ARGS__)
    #define DS3231_LOG_W(...) LOG_WRITE(DS3231_LOG_LEVEL_W, DS3231_LOG_TAG, __VA_ARGS__)
    #define DS3231_LOG_I(...) LOG_WRITE(DS3231_LOG_LEVEL_I, DS3231_LOG_TAG, __VA_ARGS__)
    #define DS3231_LOG_D(...) LOG_WRITE(DS3231_LOG_LEVEL_D, DS3231_LOG_TAG, __VA_ARGS__)
    #define DS3231_LOG_V(...) LOG_WRITE(DS3231_LOG_LEVEL_V, DS3231_LOG_TAG, __VA_ARGS__)
#else
    // ESP-IDF logging with compile-time suppression
    #define DS3231_LOG_E(...) ESP_LOGE(DS3231_LOG_TAG, __VA_ARGS__)
    #define DS3231_LOG_W(...) ESP_LOGW(DS3231_LOG_TAG, __VA_ARGS__)
    #define DS3231_LOG_I(...) ESP_LOGI(DS3231_LOG_TAG, __VA_ARGS__)
    #ifdef DS3231_DEBUG
        #define DS3231_LOG_D(...) ESP_LOGD(DS3231_LOG_TAG, __VA_ARGS__)
        #define DS3231_LOG_V(...) ESP_LOGV(DS3231_LOG_TAG, __VA_ARGS__)
    #else
        #define DS3231_LOG_D(...) ((void)0)
        #define DS3231_LOG_V(...) ((void)0)
    #endif
#endif

// Feature-specific debug helpers
#ifdef DS3231_DEBUG
    // Timing macros for performance debugging
    #define DS3231_TIME_START() TickType_t _ds3231_start = xTaskGetTickCount()
    #define DS3231_TIME_END(msg) DS3231_LOG_D("Timing: %s took %lu ms", msg, pdTICKS_TO_MS(xTaskGetTickCount() - _ds3231_start))
#else
    #define DS3231_TIME_START() ((void)0)
    #define DS3231_TIME_END(msg) ((void)0)
#endif

#endif // DS3231CONTROLLER_LOGGING_H
