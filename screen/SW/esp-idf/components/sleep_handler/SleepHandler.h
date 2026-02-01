#pragma once

#include <functional>
#include "esp_err.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "hal/hal_config.h"

// Timeout values (in milliseconds)
#define POWER_SAVE_TIMEOUT_MS   30000       // 30 seconds for power save mode
#define DEEP_SLEEP_TIMEOUT_MS   3600000     // 1 hour for deep sleep

// Wakeup pins - target specific
#if CONFIG_IDF_TARGET_ESP32S3
#define WAKEUP_PIN_TOUCH        GPIO_NUM_5   // Touch interrupt
#define WAKEUP_PIN_KNOB         GPIO_NUM_18  // UART RX from knob
#elif CONFIG_IDF_TARGET_ESP32C6
#define WAKEUP_PIN_TOUCH        GPIO_NUM_3   // Touch interrupt
#define WAKEUP_PIN_KNOB         GPIO_NUM_16  // UART RX from knob
#endif

// Callbacks
using PowerSaveModeCallback = std::function<void(bool enabled)>;
using PreSleepCallback = std::function<void()>;

#ifdef __cplusplus

class SleepHandler {
public:
    // Singleton access
    static SleepHandler* getInstance(SemaphoreHandle_t* mutex = nullptr);

    // Initialization
    esp_err_t initialize();

    // Activity tracking
    void resetActivityTime();
    void registerActivity();

    // Enable/Disable
    void enable();
    void disable();
    bool isEnabled() const { return enabled; }

    // Power save mode
    bool isPowerSaveMode() const { return power_save_enabled; }

    // Timeout configuration
    void setInactivityTimeout(uint32_t timeout_ms);
    void setPowerSaveTimeout(uint32_t timeout_ms);
    void setDeepSleepTimeout(uint32_t timeout_ms);

    // Callbacks
    void setPowerSaveModeCallback(PowerSaveModeCallback callback) { on_power_save_mode = callback; }
    void setPreSleepCallback(PreSleepCallback callback) { on_pre_sleep = callback; }

    // Main check - call regularly
    void checkActivity();

    // Manual sleep control
    void enterDeepSleep();
    void enterLightSleep(uint32_t duration_ms = 0);

    // Wakeup information
    esp_sleep_wakeup_cause_t getWakeupCause() const;
    const char* getWakeupCauseString() const;

private:
    // Private constructor for singleton
    SleepHandler(SemaphoreHandle_t* mutex);
    ~SleepHandler();

    // Prevent copying
    SleepHandler(const SleepHandler&) = delete;
    SleepHandler& operator=(const SleepHandler&) = delete;

    SemaphoreHandle_t* mutex_ptr;
    bool initialized;
    bool enabled;
    bool power_save_enabled;

    int64_t last_activity_time;
    uint32_t power_save_timeout_ms;
    uint32_t deep_sleep_timeout_ms;

    PowerSaveModeCallback on_power_save_mode;
    PreSleepCallback on_pre_sleep;

    // Configure wakeup sources
    void configureWakeupSources();
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

// C-compatible interface
typedef void* sleep_handler_handle_t;

sleep_handler_handle_t sleep_handler_get_instance(void* mutex);
esp_err_t sleep_handler_init(sleep_handler_handle_t handle);
void sleep_handler_enable(sleep_handler_handle_t handle);
void sleep_handler_disable(sleep_handler_handle_t handle);
void sleep_handler_reset_activity(sleep_handler_handle_t handle);
void sleep_handler_check_activity(sleep_handler_handle_t handle);
void sleep_handler_enter_deep_sleep(sleep_handler_handle_t handle);
bool sleep_handler_is_power_save_mode(sleep_handler_handle_t handle);

#ifdef __cplusplus
}
#endif
