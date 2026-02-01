#pragma once

#include "sdkconfig.h"

// ######################################################################################################################
// #                                           ESP32-S3 Configuration                                                    #
// ######################################################################################################################
#if CONFIG_IDF_TARGET_ESP32S3

// ========== Display (GC9A01 240x240 LCD) ===========
#define LCD_CLK_PIN     10
#define LCD_MOSI_PIN    11
#define LCD_MISO_PIN    -1
#define LCD_CS_PIN      9
#define LCD_DC_PIN      8
#define LCD_RST_PIN     14
#define LCD_BL_PIN      2

// Display properties
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

// ========== Touch Panel (CST816S) ===========
#define TP_SDA_PIN      6
#define TP_SCL_PIN      7
#define TP_INT_PIN      5
#define TP_RST_PIN      13

// ========== IMU (QMI8658) ===========
#define IMU_SDA_PIN     6   // Shared with Touch Panel
#define IMU_SCL_PIN     7   // Shared with Touch Panel
#define IMU_INT1_PIN    3
#define IMU_INT2_PIN    4

// ========== UART for Knob Communication ===========
#define KNOB_UART_NUM   UART_NUM_1
#define KNOB_TX_PIN     17
#define KNOB_RX_PIN     18
#define KNOB_BAUD_RATE  9600

// ========== Battery ADC ===========
#define BAT_ADC_PIN     1

// ========== I2C Configuration ===========
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      400000
#define I2C_MASTER_TIMEOUT_MS   1000

// ========== FreeRTOS Core Assignment ===========
#define DISPLAY_CORE    0
#define NETWORK_CORE    1
#define DUAL_CORE_AVAILABLE 1

// ######################################################################################################################
// #                                           ESP32-C6 Configuration                                                    #
// ######################################################################################################################
#elif CONFIG_IDF_TARGET_ESP32C6

// ========== Display (RM67162 454x454 AMOLED) ===========
#define LCD_CLK_PIN     19
#define LCD_MOSI_PIN    18
#define LCD_MISO_PIN    -1
#define LCD_CS_PIN      20
#define LCD_DC_PIN      22
#define LCD_RST_PIN     21
#define LCD_BL_PIN      -1  // AMOLED doesn't need backlight control

// Display properties
#define DISPLAY_WIDTH   466
#define DISPLAY_HEIGHT  466
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

// ========== Touch Panel (CST816S) ===========
#define TP_SDA_PIN      6
#define TP_SCL_PIN      7
#define TP_INT_PIN      3
#define TP_RST_PIN      -1  // Not connected on C6 board

// ========== IMU (QMI8658) ===========
#define IMU_SDA_PIN     6   // Shared with Touch Panel
#define IMU_SCL_PIN     7   // Shared with Touch Panel
#define IMU_INT1_PIN    4
#define IMU_INT2_PIN    5

// ========== UART for Knob Communication ===========
#define KNOB_UART_NUM   UART_NUM_1
#define KNOB_TX_PIN     17
#define KNOB_RX_PIN     16
#define KNOB_BAUD_RATE  9600

// ========== Battery ADC ===========
#define BAT_ADC_PIN     0

// ========== I2C Configuration ===========
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      400000
#define I2C_MASTER_TIMEOUT_MS   1000

// ========== FreeRTOS Core Assignment ===========
// ESP32-C6 is single-core, so all tasks run on core 0
#define DISPLAY_CORE    0
#define NETWORK_CORE    0
#define DUAL_CORE_AVAILABLE 0

#else
#error "Unsupported target. Please use ESP32-S3 or ESP32-C6."
#endif

// ######################################################################################################################
// #                                           Common Definitions                                                        #
// ######################################################################################################################

// Touch Panel I2C Address
#define CST816S_I2C_ADDR        0x15

// Task Priorities
#define DISPLAY_TASK_PRIORITY   2
#define NETWORK_TASK_PRIORITY   3
#define INIT_TASK_PRIORITY      4

// Task Stack Sizes
#define DISPLAY_STACK_SIZE      8192
#define NETWORK_STACK_SIZE      8192

// Timeouts
#define WIFI_CONNECT_TIMEOUT_MS     10000
#define MQTT_CONNECT_TIMEOUT_MS     5000
#define INACTIVITY_TIMEOUT_MS       30000

// NVS Namespace
#define NVS_NAMESPACE           "qnob_config"
