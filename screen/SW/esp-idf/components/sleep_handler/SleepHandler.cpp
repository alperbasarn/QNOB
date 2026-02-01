#include "SleepHandler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

static const char *TAG = "SleepHandler";

// Singleton instance
static SleepHandler* s_instance = nullptr;

SleepHandler* SleepHandler::getInstance(SemaphoreHandle_t* mutex)
{
    if (s_instance == nullptr && mutex != nullptr) {
        s_instance = new SleepHandler(mutex);
    }
    return s_instance;
}

SleepHandler::SleepHandler(SemaphoreHandle_t* mutex)
    : mutex_ptr(mutex),
      initialized(false),
      enabled(false),
      power_save_enabled(false),
      last_activity_time(0),
      power_save_timeout_ms(POWER_SAVE_TIMEOUT_MS),
      deep_sleep_timeout_ms(DEEP_SLEEP_TIMEOUT_MS),
      on_power_save_mode(nullptr),
      on_pre_sleep(nullptr)
{
    last_activity_time = esp_timer_get_time() / 1000;
}

SleepHandler::~SleepHandler()
{
    s_instance = nullptr;
}

esp_err_t SleepHandler::initialize()
{
    ESP_LOGI(TAG, "Initializing sleep handler...");

    // Log wakeup cause if waking from sleep
    esp_sleep_wakeup_cause_t wakeup_cause = getWakeupCause();
    if (wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Wakeup cause: %s", getWakeupCauseString());
    }

    resetActivityTime();
    initialized = true;

    ESP_LOGI(TAG, "Sleep handler initialized");
    return ESP_OK;
}

void SleepHandler::resetActivityTime()
{
    last_activity_time = esp_timer_get_time() / 1000;
}

void SleepHandler::registerActivity()
{
    resetActivityTime();

    // Exit power save mode if active
    if (power_save_enabled) {
        power_save_enabled = false;
        ESP_LOGI(TAG, "Exiting power save mode due to activity");

        if (on_power_save_mode) {
            on_power_save_mode(false);
        }
    }
}

void SleepHandler::enable()
{
    enabled = true;
    resetActivityTime();
    ESP_LOGI(TAG, "Sleep handler enabled");
}

void SleepHandler::disable()
{
    enabled = false;
    ESP_LOGI(TAG, "Sleep handler disabled");
}

void SleepHandler::setInactivityTimeout(uint32_t timeout_ms)
{
    deep_sleep_timeout_ms = timeout_ms;
}

void SleepHandler::setPowerSaveTimeout(uint32_t timeout_ms)
{
    power_save_timeout_ms = timeout_ms;
}

void SleepHandler::setDeepSleepTimeout(uint32_t timeout_ms)
{
    deep_sleep_timeout_ms = timeout_ms;
}

void SleepHandler::checkActivity()
{
    if (!enabled || !initialized) {
        return;
    }

    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t inactivity_time = current_time - last_activity_time;

    // Check for power save mode threshold
    if (!power_save_enabled && inactivity_time >= power_save_timeout_ms &&
        inactivity_time < deep_sleep_timeout_ms) {

        ESP_LOGI(TAG, "Entering power save mode (inactive for %lld ms)", inactivity_time);
        power_save_enabled = true;

        if (on_power_save_mode) {
            on_power_save_mode(true);
        }
    }

    // Check for deep sleep threshold
    if (inactivity_time >= deep_sleep_timeout_ms) {
        ESP_LOGI(TAG, "Inactivity timeout reached (%lld ms), entering deep sleep", inactivity_time);
        enterDeepSleep();
    }
}

void SleepHandler::configureWakeupSources()
{
    ESP_LOGI(TAG, "Configuring wakeup sources...");

#if CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3: Use EXT1 wakeup with RTC GPIO
    uint64_t wakeup_pin_mask = 0;

    // Configure touch wakeup pin
    if (rtc_gpio_is_valid_gpio(WAKEUP_PIN_TOUCH)) {
        rtc_gpio_init(WAKEUP_PIN_TOUCH);
        rtc_gpio_set_direction(WAKEUP_PIN_TOUCH, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(WAKEUP_PIN_TOUCH);
        rtc_gpio_pulldown_dis(WAKEUP_PIN_TOUCH);
        wakeup_pin_mask |= (1ULL << WAKEUP_PIN_TOUCH);
        ESP_LOGI(TAG, "Touch wakeup pin %d configured", WAKEUP_PIN_TOUCH);
    }

    // Configure knob UART RX wakeup pin
    if (rtc_gpio_is_valid_gpio(WAKEUP_PIN_KNOB)) {
        rtc_gpio_init(WAKEUP_PIN_KNOB);
        rtc_gpio_set_direction(WAKEUP_PIN_KNOB, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(WAKEUP_PIN_KNOB);
        rtc_gpio_pulldown_dis(WAKEUP_PIN_KNOB);
        wakeup_pin_mask |= (1ULL << WAKEUP_PIN_KNOB);
        ESP_LOGI(TAG, "Knob wakeup pin %d configured", WAKEUP_PIN_KNOB);
    }

    if (wakeup_pin_mask != 0) {
        esp_sleep_enable_ext1_wakeup(wakeup_pin_mask, ESP_EXT1_WAKEUP_ANY_LOW);
        ESP_LOGI(TAG, "EXT1 wakeup configured with mask 0x%llX", wakeup_pin_mask);
    }

#elif CONFIG_IDF_TARGET_ESP32C6
    // ESP32-C6: Use GPIO wakeup (different API)
    // Note: ESP32-C6 has different deep sleep wakeup capabilities

    // Enable GPIO wakeup
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WAKEUP_PIN_TOUCH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_LOW_LEVEL,
    };
    gpio_config(&io_conf);

    esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKEUP_PIN_TOUCH), ESP_GPIO_WAKEUP_GPIO_LOW);
    ESP_LOGI(TAG, "GPIO wakeup configured for pin %d", WAKEUP_PIN_TOUCH);
#endif

    // Also enable timer wakeup as backup (wake after 1 hour max)
    esp_sleep_enable_timer_wakeup(3600ULL * 1000000ULL);  // 1 hour in microseconds
    ESP_LOGI(TAG, "Timer wakeup configured (1 hour)");
}

void SleepHandler::enterDeepSleep()
{
    if (!enabled) {
        ESP_LOGW(TAG, "Sleep handler disabled, not entering deep sleep");
        return;
    }

    ESP_LOGI(TAG, "Preparing for deep sleep...");

    // Call pre-sleep callback (e.g., to turn off display)
    if (on_pre_sleep) {
        on_pre_sleep();
    }

    // Configure wakeup sources
    configureWakeupSources();

    // Flush logs
    ESP_LOGI(TAG, "Entering deep sleep now...");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Enter deep sleep
    esp_deep_sleep_start();

    // This line is never reached
}

void SleepHandler::enterLightSleep(uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Entering light sleep for %lu ms", duration_ms);

    if (duration_ms > 0) {
        esp_sleep_enable_timer_wakeup(duration_ms * 1000ULL);
    }

    configureWakeupSources();

    esp_light_sleep_start();

    // Light sleep returns here after wakeup
    ESP_LOGI(TAG, "Woke from light sleep: %s", getWakeupCauseString());
    resetActivityTime();
}

esp_sleep_wakeup_cause_t SleepHandler::getWakeupCause() const
{
    return esp_sleep_get_wakeup_cause();
}

const char* SleepHandler::getWakeupCauseString() const
{
    switch (getWakeupCause()) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:    return "Undefined (power-on)";
        case ESP_SLEEP_WAKEUP_ALL:          return "All";
        case ESP_SLEEP_WAKEUP_EXT0:         return "EXT0 (single GPIO)";
        case ESP_SLEEP_WAKEUP_EXT1:         return "EXT1 (multiple GPIO)";
        case ESP_SLEEP_WAKEUP_TIMER:        return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:     return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:          return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO:         return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:         return "UART";
        default:                            return "Unknown";
    }
}

// C-compatible interface
extern "C" {

sleep_handler_handle_t sleep_handler_get_instance(void* mutex)
{
    return SleepHandler::getInstance(static_cast<SemaphoreHandle_t*>(mutex));
}

esp_err_t sleep_handler_init(sleep_handler_handle_t handle)
{
    return static_cast<SleepHandler*>(handle)->initialize();
}

void sleep_handler_enable(sleep_handler_handle_t handle)
{
    static_cast<SleepHandler*>(handle)->enable();
}

void sleep_handler_disable(sleep_handler_handle_t handle)
{
    static_cast<SleepHandler*>(handle)->disable();
}

void sleep_handler_reset_activity(sleep_handler_handle_t handle)
{
    static_cast<SleepHandler*>(handle)->resetActivityTime();
}

void sleep_handler_check_activity(sleep_handler_handle_t handle)
{
    static_cast<SleepHandler*>(handle)->checkActivity();
}

void sleep_handler_enter_deep_sleep(sleep_handler_handle_t handle)
{
    static_cast<SleepHandler*>(handle)->enterDeepSleep();
}

bool sleep_handler_is_power_save_mode(sleep_handler_handle_t handle)
{
    return static_cast<SleepHandler*>(handle)->isPowerSaveMode();
}

}
