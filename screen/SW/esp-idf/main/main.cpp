/*
 * QNOB Screen Application - ESP-IDF Version
 *
 * Main entry point for the QNOB smart knob display application.
 * Supports both ESP32-S3 (GC9A01 LCD) and ESP32-C6 (RM67162 AMOLED).
 */
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

// HAL and hardware components
#include "hal/hal_config.h"
#include "hal/hal_init.h"

// Display
#include "display/LGFX_Config.hpp"

// Hardware interfaces
#include "touch_panel/TouchPanel.h"
#include "knob_controller/KnobController.h"
#include "storage/NVSManager.h"

static const char *TAG = "QNOB";

// Global objects
static LGFX* gfx = nullptr;
static TouchPanel* touchPanel = nullptr;
static KnobController* knobController = nullptr;
static NVSManager* nvsManager = nullptr;

// Task handles
static TaskHandle_t displayTaskHandle = nullptr;

// Display task - handles screen updates and touch input
void displayTask(void* parameter)
{
    ESP_LOGI(TAG, "Display task started on core %d", xPortGetCoreID());

    int frame_count = 0;
    int64_t last_fps_time = esp_timer_get_time();

    while (true) {
        // Handle touch input
        if (touchPanel) {
            touchPanel->handleTouchPanel();

            if (touchPanel->getHasNewPress()) {
                ESP_LOGI(TAG, "Touch pressed at (%d, %d)",
                         touchPanel->getTouchX(), touchPanel->getTouchY());

                // Draw a circle at touch position
                gfx->fillCircle(touchPanel->getTouchX(), touchPanel->getTouchY(), 10, TFT_RED);
            }

            if (touchPanel->getHasNewRelease()) {
                ESP_LOGI(TAG, "Touch released");
            }
        }

        // Handle knob input
        if (knobController) {
            knobController->update();

            if (knobController->getHasNewMessage()) {
                ESP_LOGI(TAG, "Knob message: %s", knobController->getReceivedCommand().c_str());
            }
        }

        // Update display (simple animation for testing)
        frame_count++;
        int64_t current_time = esp_timer_get_time();
        if (current_time - last_fps_time >= 1000000) {  // 1 second
            ESP_LOGD(TAG, "FPS: %d", frame_count);
            frame_count = 0;
            last_fps_time = current_time;
        }

        vTaskDelay(pdMS_TO_TICKS(16));  // ~60 FPS target
    }
}

// Draw initialization screen
void drawInitScreen(int progress)
{
    int centerX = DISPLAY_WIDTH / 2;
    int centerY = DISPLAY_HEIGHT / 2;

    // Draw progress arc
    int radius = DISPLAY_WIDTH / 3;
    float angle = (progress / 100.0f) * 360.0f;

    gfx->fillArc(centerX, centerY, radius + 5, radius - 5, 270, 270 + (int)angle, TFT_CYAN);

    // Draw progress text
    gfx->setTextColor(TFT_WHITE);
    gfx->setTextDatum(middle_center);
    gfx->setTextSize(2);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", progress);
    gfx->drawString(buf, centerX, centerY);

    // Draw device info
    gfx->setTextSize(1);
    gfx->drawString("QNOB", centerX, centerY + 40);

#if CONFIG_IDF_TARGET_ESP32S3
    gfx->drawString("ESP32-S3", centerX, centerY + 55);
#elif CONFIG_IDF_TARGET_ESP32C6
    gfx->drawString("ESP32-C6", centerX, centerY + 55);
#endif
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "QNOB Screen Application Starting...");
    ESP_LOGI(TAG, "===========================================");

#if CONFIG_IDF_TARGET_ESP32S3
    ESP_LOGI(TAG, "Target: ESP32-S3 with GC9A01 LCD (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
#elif CONFIG_IDF_TARGET_ESP32C6
    ESP_LOGI(TAG, "Target: ESP32-C6 with RM67162 AMOLED (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif

    // Initialize HAL
    ESP_LOGI(TAG, "Initializing HAL...");
    ESP_ERROR_CHECK(hal_init());

    // Initialize NVS storage
    ESP_LOGI(TAG, "Initializing NVS storage...");
    nvsManager = new NVSManager();
    ESP_ERROR_CHECK(nvsManager->begin());
    ESP_LOGI(TAG, "Device name: %s", nvsManager->deviceName.c_str());

    // Initialize display
    ESP_LOGI(TAG, "Initializing display...");
    gfx = new LGFX();
    gfx->init();
    gfx->setRotation(0);
    gfx->setBrightness(255);
    gfx->fillScreen(TFT_BLACK);

    // Show initialization progress
    drawInitScreen(10);

    // Initialize touch panel
    ESP_LOGI(TAG, "Initializing touch panel...");
    touchPanel = new TouchPanel(TP_SDA_PIN, TP_SCL_PIN, TP_RST_PIN, TP_INT_PIN);
    if (touchPanel->initialize() != ESP_OK) {
        ESP_LOGW(TAG, "Touch panel initialization failed - continuing without touch");
    }
    drawInitScreen(40);

    // Initialize knob controller
    ESP_LOGI(TAG, "Initializing knob controller...");
    knobController = new KnobController(KNOB_TX_PIN, KNOB_RX_PIN, KNOB_UART_NUM);
    if (knobController->begin(KNOB_BAUD_RATE) != ESP_OK) {
        ESP_LOGW(TAG, "Knob controller initialization failed - continuing without knob");
    }
    drawInitScreen(70);

    // Show completion
    drawInitScreen(100);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Clear screen and show ready state
    gfx->fillScreen(TFT_BLACK);

    int centerX = DISPLAY_WIDTH / 2;
    int centerY = DISPLAY_HEIGHT / 2;

    // Draw a simple UI
    gfx->drawCircle(centerX, centerY, DISPLAY_WIDTH / 3, TFT_CYAN);
    gfx->setTextColor(TFT_WHITE);
    gfx->setTextDatum(middle_center);
    gfx->setTextSize(2);
    gfx->drawString("READY", centerX, centerY);
    gfx->setTextSize(1);
    gfx->drawString("Touch to test", centerX, centerY + 30);

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "Initialization complete!");
    ESP_LOGI(TAG, "===========================================");

    // Start display task
#if DUAL_CORE_AVAILABLE
    xTaskCreatePinnedToCore(displayTask, "DisplayTask", DISPLAY_STACK_SIZE,
                            nullptr, DISPLAY_TASK_PRIORITY, &displayTaskHandle, DISPLAY_CORE);
#else
    xTaskCreate(displayTask, "DisplayTask", DISPLAY_STACK_SIZE,
                nullptr, DISPLAY_TASK_PRIORITY, &displayTaskHandle);
#endif

    // Main loop - can handle other tasks or enter sleep
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
