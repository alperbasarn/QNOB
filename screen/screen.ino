#include <Arduino_GFX_Library.h>
#include "DisplayController.h"
#include "TouchPanel.h"
#include "WiFiTCPClient.h"
#include "EEPROMManager.h"
#include "CommandHandler.h"
#include "KnobController.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>

// GPIO pins for wakeup
#define WAKEUP_PIN_LOW GPIO_NUM_5  // Wakeup when touch screen is pressed and interrupt pin pulls down
#define WAKEUP_PIN_RX GPIO_NUM_16  // Wakeup when motor controller sends a serial command and RX (Pin 15) goes LOW

// Knob communication pins
#define KNOB_RX_PIN 16
#define KNOB_TX_PIN 15

int lastActionTime = 0;
WiFiTCPClient* tcpClient = nullptr;
Arduino_DataBus* bus = nullptr;
Arduino_GFX* gfx = nullptr;
DisplayController* displayController = nullptr;
EEPROMManager eepromManager;
KnobController* knobController = nullptr;
CommandHandler* commandHandler = nullptr;

// Store wake reason in RTC memory so it persists during deep sleep
RTC_DATA_ATTR bool isWakingFromDeepSleep = false;

void goToSleep() {
  Serial.println("Configuring wakeup sources...");
  
  // Set flag to indicate next boot will be from deep sleep
  isWakingFromDeepSleep = true;
  
  // Turn off display and other peripherals to save power
  if (displayController) {
    displayController->turnDisplayOff();
  }
  
  // Configure multiple wakeup sources
  esp_sleep_enable_ext1_wakeup(
    (1ULL << WAKEUP_PIN_LOW) | (1ULL << WAKEUP_PIN_RX),  // Bitmask for both pins
    ESP_EXT1_WAKEUP_ALL_LOW                              // Trigger when ALL go LOW
  );

  // Configure RTC GPIO for WAKEUP_PIN_LOW
  rtc_gpio_init(WAKEUP_PIN_LOW);
  rtc_gpio_set_direction(WAKEUP_PIN_LOW, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(WAKEUP_PIN_LOW);
  rtc_gpio_pulldown_dis(WAKEUP_PIN_LOW);

  // Configure RTC GPIO for WAKEUP_PIN_RX
  rtc_gpio_init(WAKEUP_PIN_RX);
  rtc_gpio_set_direction(WAKEUP_PIN_RX, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(WAKEUP_PIN_RX);  // Enable pull-up to keep RX idle state HIGH
  rtc_gpio_pulldown_dis(WAKEUP_PIN_RX);

  Serial.println("Entering deep sleep...");
  delay(1000);

  // Enter deep sleep
  esp_deep_sleep_start();
}

void cleanupResources() {
  // Free all allocated resources to prevent memory issues
  if (commandHandler) {
    delete commandHandler;
    commandHandler = nullptr;
  }
  
  if (knobController) {
    delete knobController;
    knobController = nullptr;
  }
  
  if (displayController) {
    delete displayController;
    displayController = nullptr;
  }
  
  if (tcpClient) {
    delete tcpClient;
    tcpClient = nullptr;
  }
  
  if (gfx) {
    delete gfx;
    gfx = nullptr;
  }
  
  if (bus) {
    delete bus;
    bus = nullptr;
  }
}

void handleSleep() {
  if (displayController && (displayController->getHasNewEvent() || 
      (knobController && knobController->getHasNewMessage()) || 
      (commandHandler && commandHandler->getHasNewMessage()))) {
    lastActionTime = millis();
  }
  
  if (millis() - lastActionTime > 30000) {  // Check for inactivity (30 seconds)
    goToSleep();
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Check wake-up cause and handle RTC GPIOs
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool isDeepSleepWake = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) || isWakingFromDeepSleep;
  
  if (isDeepSleepWake) {
    Serial.println("\n\n----- Waking from deep sleep -----");
    
    // Deinitialize RTC GPIOs that were configured for wake-up
    rtc_gpio_deinit(WAKEUP_PIN_LOW);
    rtc_gpio_deinit(WAKEUP_PIN_RX);
    
    // Reset wake flag for next cycle
    isWakingFromDeepSleep = false;
  } else {
    Serial.println("\n\n----- Normal boot initialization -----");
  }
  
  // Clean up any previously allocated resources (important for deep sleep wake)
  cleanupResources();
  
  // Initialize display first to show progress
  bus = create_default_Arduino_DataBus();
  gfx = new Arduino_GC9A01(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);

  // Create display controller but without other components
  displayController = new DisplayController(gfx, 6, 7, 19, 5, nullptr);

  // Initialize the display - this will set mode to INITIALIZATION
  displayController->initScreen();
  displayController->updateInitProgress(5);

  // Show progress for EEPROM initialization
  Serial.println("Initializing EEPROM...");
  displayController->updateInitProgress(10);
  eepromManager.begin();
  Serial.println("EEPROM initialized with size: " + String(EEPROM_SIZE) + " bytes");
  displayController->updateInitProgress(20);

  // Show progress for WiFi initialization
  Serial.println("Creating TCP/WiFi client...");
  displayController->updateInitProgress(30);
  tcpClient = new WiFiTCPClient(&eepromManager);
  Serial.println("WiFi TCP Client created");

  // Update display controller with TCP client reference
  displayController->setTCPClient(tcpClient);
  displayController->updateInitProgress(40);

  // Initialize knob controller
  Serial.println("Initializing knob controller...");
  displayController->updateInitProgress(50);
  knobController = new KnobController(KNOB_RX_PIN, KNOB_TX_PIN);
  knobController->begin(9600);
  Serial.println("Knob Controller initialized");
  displayController->updateInitProgress(60);

  // Register knob controller with display controller
  Serial.println("Registering knob controller with display...");
  displayController->registerKnobController(knobController);
  Serial.println("Display Controller initialized with Knob Controller");
  displayController->updateInitProgress(70);

  // Create and initialize command handler
  Serial.println("Setting up command handler...");
  commandHandler = new CommandHandler(displayController, &eepromManager, tcpClient);
  commandHandler->initialize();
  Serial.println("Command Handler initialized");
  displayController->updateInitProgress(80);

  // Register knob controller with command handler
  Serial.println("Finalizing controller setup...");
  commandHandler->registerKnobController(knobController);
  displayController->updateInitProgress(85);

  // Initialize network components
  Serial.println("Starting network components...");
  tcpClient->initialize();
  Serial.println("Network components initialized");
  displayController->updateInitProgress(95);
  
  // Initialize network components
  Serial.println("Finalizing connections...");
  tcpClient->update();
  Serial.println("Connections done");
  displayController->updateInitProgress(99);

  // All systems initialized
  Serial.println("----- Initialization complete -----");
  displayController->updateInitProgress(100);
  delay(1000);  // Show 100% completion for a little longer

  // Set to info mode
  displayController->setMode(INFO);

  // Initialize the last action time
  lastActionTime = millis();
}

void loop() {
  // Safety check for null pointers
  if (!displayController || !knobController || !commandHandler || !tcpClient) {
    Serial.println("Error: One or more critical components are null. Reinitializing...");
    // Attempt to clean up and restart
    cleanupResources();
    setup();
    return;
  }

  // Use a staggered update approach to prioritize responsiveness
  // TCP must be checked every cycle for best response time
  tcpClient->update();

  // Update one non-network component per cycle (round-robin)
  static uint8_t updateComponent = 0;

  switch (updateComponent) {
    case 0:
      // Display updates are less time-critical
      displayController->update();
      break;

    case 1:
      // Knob commands should be checked frequently
      knobController->update();
      break;

    case 2:
      // Command processing should be checked every few cycles
      commandHandler->update();
      break;

    case 3:
      // Sleep handling is least time-critical
      handleSleep();
      break;
  }

  // Move to next component for next cycle
  updateComponent = (updateComponent + 1) % 4;

  // Small delay to prevent CPU hogging and reduce power consumption
  // This is the only delay in the main loop and it's very short
  delayMicroseconds(10);
}