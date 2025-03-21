#include <Arduino_GFX_Library.h>
#include "DisplayController.h"
#include "TouchPanel.h"
#include "WiFiTCPClient.h"
#include "EEPROMManager.h"
#include "CommandHandler.h"
#include "KnobController.h"
#include "SleepHandler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Knob communication pins
#define KNOB_RX_PIN 16
#define KNOB_TX_PIN 15

// Core definitions
#define NETWORK_CORE 1  // Core 1 for network/TCP operations (higher priority)
#define DISPLAY_CORE 0  // Core 0 for display and other operations (lower priority)

// Task priorities
#define NETWORK_TASK_PRIORITY 2  // Higher priority for network operations
#define DISPLAY_TASK_PRIORITY 1  // Lower priority for display operations
#define INIT_TASK_PRIORITY 3     // Highest priority for initialization

// Task stack sizes
#define NETWORK_STACK_SIZE 8192
#define DISPLAY_STACK_SIZE 8192

// Global variables
WiFiTCPClient* tcpClient = NULL;
Arduino_DataBus* bus = create_default_Arduino_DataBus();
Arduino_GFX* gfx = new Arduino_GC9A01(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);
DisplayController* displayController = NULL;
EEPROMManager eepromManager;
KnobController* knobController = NULL;
CommandHandler* commandHandler = NULL;
SleepHandler* sleepHandler = NULL;

// Task handles
TaskHandle_t displayTask = NULL;
TaskHandle_t networkTask = NULL;
TaskHandle_t initDisplayTask = NULL;
TaskHandle_t initNetworkTask = NULL;

// Mutex and semaphores for synchronizing access to shared resources
SemaphoreHandle_t xMutex = NULL;
SemaphoreHandle_t xEEPROMSemaphore = NULL;     // Signal EEPROM is initialized
SemaphoreHandle_t xTCPClientSemaphore = NULL;  // Signal TCP client is created

// Initialization flags
volatile bool displayInitialized = false;
volatile bool networkInitialized = false;
volatile bool initComplete = false;
volatile bool setupComplete = false;

// Function to print memory statistics
void printMemoryStats() {
  Serial.println("Memory Statistics:");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  if (psramFound()) {
    Serial.printf("Total PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  }
  Serial.printf("Minimum free heap: %u bytes\n", ESP.getMinFreeHeap());
}

// Main Display Task (runs on DISPLAY_CORE continuously)
void displayTask_func(void* parameter) {
  Serial.println("[Display Task] Starting main display task on core " + String(xPortGetCoreID()));

  // Main display task loop
  for (;;) {
    // Take mutex when accessing shared resources
    if (xSemaphoreTake(xMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
      displayController->update();

      // Only update knobController if it has been initialized
      if (knobController) {
        knobController->update();
      }

      if (sleepHandler) {
        sleepHandler->checkActivity();
      }
      xSemaphoreGive(xMutex);
    }
    // If both initialization tasks are complete, set the mode to INFO
    if (displayInitialized && networkInitialized && !initComplete) {
      if (xSemaphoreTake(xMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        displayController->updateInitProgress(100);
        delay(500);  // Small delay for user to see 100% completion
        displayController->setMode(INFO);
        initComplete = true;
        // Check for sleep if sleep handler is initialized
        xSemaphoreGive(xMutex);
      }
    }

    // Small delay to yield to other tasks
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Main Network Task (runs on NETWORK_CORE continuously)
void networkTask_func(void* parameter) {
  Serial.println("[Network Task] Starting main network task on core " + String(xPortGetCoreID()));

  unsigned long lastMemCheckTime = 0;

  // Main network task loop
  for (;;) {
    // Only process network-related operations once tcpClient and commandHandler are initialized
    if (tcpClient && commandHandler) {
      // Take mutex when accessing shared resources
      if (xSemaphoreTake(xMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        tcpClient->update();
        commandHandler->update();

        // Print memory stats every 30 seconds
        unsigned long currentMillis = millis();
        if (currentMillis - lastMemCheckTime > 30000) {
          printMemoryStats();
          lastMemCheckTime = currentMillis;
        }

        xSemaphoreGive(xMutex);
      }
    }

    // Small delay to yield to other tasks
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Initialize Display Components Task (runs on DISPLAY_CORE)
void initDisplayTask_func(void* parameter) {
  Serial.println("[Display Init] Starting display initialization on core " + String(xPortGetCoreID()));

  // Initialize display controller first to show progress
  displayController = new DisplayController(gfx, 6, 7, 19, 5, nullptr);

  // Initialize the display
  displayController->initScreen();

  // Start the display task immediately so it can update the screen
  xTaskCreatePinnedToCore(
    displayTask_func,      /* Task function */
    "DisplayTask",         /* Name of task */
    DISPLAY_STACK_SIZE,    /* Stack size (bytes) */
    NULL,                  /* Parameter */
    DISPLAY_TASK_PRIORITY, /* Task priority */
    &displayTask,          /* Task handle */
    DISPLAY_CORE           /* Core where the task should run */
  );

  // Display first progress update
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    displayController->updateInitProgress(10);
    xSemaphoreGive(xMutex);
  }

  // Wait for EEPROM initialization from the network task
  Serial.println("[Display Init] Waiting for EEPROM initialization...");
  xSemaphoreTake(xEEPROMSemaphore, portMAX_DELAY);

  // Display progress after EEPROM init
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    displayController->updateInitProgress(30);
    xSemaphoreGive(xMutex);
  }

  // Initialize knob controller
  Serial.println("[Display Init] Initializing knob controller...");
  knobController = new KnobController(KNOB_RX_PIN, KNOB_TX_PIN);
  knobController->begin(9600);
  Serial.println("[Display Init] Knob Controller initialized");

  // Update progress
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    displayController->updateInitProgress(50);
    xSemaphoreGive(xMutex);
  }

  // Register knob controller with display controller
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    displayController->registerKnobController(knobController);
    displayController->updateInitProgress(60);
    xSemaphoreGive(xMutex);
  }
  Serial.println("[Display Init] Display Controller initialized with Knob Controller");

  // Wait for TCP client creation from network task
  Serial.println("[Display Init] Waiting for TCP Client creation...");
  xSemaphoreTake(xTCPClientSemaphore, portMAX_DELAY);

  Serial.println("[Display Init] Setting up command handler...");
  commandHandler = new CommandHandler(displayController, &eepromManager, tcpClient);
  commandHandler->initialize();
  Serial.println("[Display Init] Command Handler initialized");

  // Update progress
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    displayController->updateInitProgress(70);
    xSemaphoreGive(xMutex);
  }

  // Register knob controller with command handler
  commandHandler->registerKnobController(knobController);
  Serial.println("[Display Init] Controller setup complete");

  // Update TCP client in display controller
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    displayController->setTCPClient(tcpClient);
    displayController->updateInitProgress(80);
    xSemaphoreGive(xMutex);
  }

  Serial.println("[Display Init] Display initialization complete");

  // Signal display initialization is complete
  displayInitialized = true;

  // Delete the init task
  vTaskDelete(NULL);
}

// Initialize Network Components Task (runs on NETWORK_CORE)
void initNetworkTask_func(void* parameter) {
  Serial.println("[Network Init] Starting network initialization on core " + String(xPortGetCoreID()));

  // Initialize EEPROM first (needed by both display and network)
  Serial.println("[Network Init] Initializing EEPROM...");
  eepromManager.begin();
  Serial.println("[Network Init] EEPROM initialized with size: " + String(EEPROM_SIZE) + " bytes");

  // Signal EEPROM is initialized - this unblocks the display initialization
  xSemaphoreGive(xEEPROMSemaphore);

  // Create the network task early
  xTaskCreatePinnedToCore(
    networkTask_func,      /* Task function */
    "NetworkTask",         /* Name of task */
    NETWORK_STACK_SIZE,    /* Stack size (bytes) */
    NULL,                  /* Parameter */
    NETWORK_TASK_PRIORITY, /* Task priority */
    &networkTask,          /* Task handle */
    NETWORK_CORE           /* Core where the task should run */
  );

  // Create WiFi/TCP client
  Serial.println("[Network Init] Creating TCP/WiFi client...");
  tcpClient = new WiFiTCPClient(&eepromManager);
  Serial.println("[Network Init] WiFi TCP Client created");

  // Signal TCP client is created - this unblocks the display initialization
  xSemaphoreGive(xTCPClientSemaphore);

  // Initialize WiFi and TCP components
  Serial.println("[Network Init] Starting network components...");
  tcpClient->initialize();
  Serial.println("[Network Init] Network components initialized");

  // Signal network initialization is complete (NOT waiting for display)
  networkInitialized = true;

  Serial.println("[Network Init] Network initialization complete");

  // Delete the init task
  vTaskDelete(NULL);
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n\n----- Starting device initialization -----");

  // Check PSRAM
  if (psramFound()) {
    Serial.println("PSRAM found and initialized");
    Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("No PSRAM detected");
  }

  // Create synchronization primitives
  xMutex = xSemaphoreCreateMutex();
  xEEPROMSemaphore = xSemaphoreCreateBinary();
  xTCPClientSemaphore = xSemaphoreCreateBinary();

  // Initialize sleep handler (disabled by default)
  sleepHandler = SleepHandler::getInstance(&xMutex);
  sleepHandler->setInactivityTimeout(30000);  // 30 seconds
  sleepHandler->disable();                    // Explicitly disable at start

  // Create initialization tasks
  xTaskCreatePinnedToCore(
    initDisplayTask_func, /* Task function */
    "InitDisplayTask",    /* Name of task */
    DISPLAY_STACK_SIZE,   /* Stack size (bytes) */
    NULL,                 /* Parameter */
    INIT_TASK_PRIORITY,   /* Task priority */
    &initDisplayTask,     /* Task handle */
    DISPLAY_CORE          /* Core where the task should run */
  );

  xTaskCreatePinnedToCore(
    initNetworkTask_func, /* Task function */
    "InitNetworkTask",    /* Name of task */
    NETWORK_STACK_SIZE,   /* Stack size (bytes) */
    NULL,                 /* Parameter */
    INIT_TASK_PRIORITY,   /* Task priority */
    &initNetworkTask,     /* Task handle */
    NETWORK_CORE          /* Core where the task should run */
  );

  Serial.println("All initialization tasks created successfully!");

  // Setup is complete - sleep handler will be enabled when initialization is fully completed
  setupComplete = true;
  // Enable sleep handler now that initialization is complete
  if (sleepHandler) {
    sleepHandler->enable();
  }
}

void loop() {
  // Empty loop - all work is done in tasks
  vTaskDelay(0);
}