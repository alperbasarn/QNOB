#include "SleepHandler.h"

// Static instance for singleton pattern
static SleepHandler* instance = nullptr;

SleepHandler* SleepHandler::getInstance(SemaphoreHandle_t* mutex) {
  if (instance == nullptr && mutex != nullptr) {
    instance = new SleepHandler(mutex);
  }
  return instance;
}

SleepHandler::SleepHandler(SemaphoreHandle_t* mutex)
  : displayController(nullptr),
    commandHandler(nullptr),
    lastActivityTime(0),
    inactivityTimeout(30000),  // Default 30 seconds
    enabled(false),            // Disabled by default
    mutexPtr(mutex) {

  // Initialize the activity timer
  lastActivityTime = millis();
}

SleepHandler::~SleepHandler() {
  // Nothing to clean up as we don't own the pointers
}

void SleepHandler::registerControllers(DisplayController* display, CommandHandler* command) {
  displayController = display;
  commandHandler = command;
}

void SleepHandler::setInactivityTimeout(unsigned long timeoutMs) {
  inactivityTimeout = timeoutMs;
}

void SleepHandler::resetActivityTime() {
  lastActivityTime = millis();
  Serial.println("Sleep timer reset");
}

void SleepHandler::enable() {
  enabled = true;
  resetActivityTime();  // Reset timer when enabled
  Serial.println("Sleep handler enabled");
}

void SleepHandler::disable() {
  enabled = false;
  Serial.println("Sleep handler disabled");
}

bool SleepHandler::isEnabled() {
  return enabled;
}

void SleepHandler::checkActivity() {
  // Skip checking if disabled or missing required controllers
  if (!enabled || !displayController || !commandHandler) {
    Serial.println("sleep handler not enabled or not initialized properly!");
    return;
  }

  bool hasActivity = false;

  // Take mutex to safely access shared resources
  if (*mutexPtr && xSemaphoreTake(*mutexPtr, 10 / portTICK_PERIOD_MS) == pdTRUE) {
    hasActivity = displayController->getHasNewEvent() || commandHandler->getHasNewMessage();

    if (hasActivity) {
      resetActivityTime();
    }

    xSemaphoreGive(*mutexPtr);
  }

  // Check for inactivity timeout
  int inactivityTime = millis() - lastActivityTime;
  if (inactivityTime > inactivityTimeout) {
    Serial.println("Inactivity timeout reached, entering deep sleep");
    enterDeepSleep();
  }
  //Serial.println("Sleep handler update: inactivity time:" + String(inactivityTime) + "|| inactivity timeout duration:" + String(inactivityTimeout));
}

void SleepHandler::configureWakeupPins() {
  Serial.println("Configuring wakeup sources...");

  // Configure multiple wakeup sources (both pins must be LOW to wake up)
  esp_sleep_enable_ext1_wakeup(
    (1ULL << WAKEUP_PIN_TOUCH) | (1ULL << WAKEUP_PIN_KNOB),  // Bitmask for both pins
    ESP_EXT1_WAKEUP_ALL_LOW                                  // Trigger when ALL go LOW
  );

  // Configure RTC GPIO for WAKEUP_PIN_TOUCH
  rtc_gpio_init(WAKEUP_PIN_TOUCH);
  rtc_gpio_set_direction(WAKEUP_PIN_TOUCH, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(WAKEUP_PIN_TOUCH);
  rtc_gpio_pulldown_dis(WAKEUP_PIN_TOUCH);

  // Configure RTC GPIO for WAKEUP_PIN_KNOB
  rtc_gpio_init(WAKEUP_PIN_KNOB);
  rtc_gpio_set_direction(WAKEUP_PIN_KNOB, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(WAKEUP_PIN_KNOB);  // Enable pull-up to keep RX idle state HIGH
  rtc_gpio_pulldown_dis(WAKEUP_PIN_KNOB);
}

void SleepHandler::enterDeepSleep() {
  // Skip if disabled
  if (!enabled) {
    return;
  }

  // Turn off display if possible
  if (displayController) {
    displayController->setDisplayOff();
  }

  // Configure wakeup pins
  configureWakeupPins();

  Serial.println("Entering deep sleep...");
  delay(100);  // Small delay to ensure Serial output completes

  // Enter deep sleep mode
  esp_deep_sleep_start();
}