#include "Definitions.h"

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

      xSemaphoreGive(xMutex);
    }
    // If both initialization tasks are complete, set the mode to INFO
    if (displayInitialized && networkInitialized && !initComplete) {
      if (xSemaphoreTake(xMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        displayController->updateInitProgress(100);
        delay(200);  // Small delay for user to see 100% completion
        displayController->setMode(INFO);
        initComplete = true;

        // Enable sleep handler now that initialization is complete
        if (sleepHandler) {
          sleepHandler->registerControllers(displayController, commandHandler);
          sleepHandler->enable();
        }

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

      // Check for sleep if sleep handler is initialized
      if (sleepHandler && initComplete) {
        sleepHandler->checkActivity();
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
}

void loop() {
  // Empty loop - all work is done in tasks
  vTaskDelay(0);
}