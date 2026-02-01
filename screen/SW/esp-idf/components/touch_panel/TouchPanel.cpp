#include "TouchPanel.h"
#include "hal/hal_config.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TouchPanel";

TouchPanel::TouchPanel(int sda, int scl, int rst, int irq)
    : sda_pin(sda), scl_pin(scl), rst_pin(rst), irq_pin(irq),
      i2c_addr(CST816S_I2C_ADDR), initialized(false),
      first_press(true), first_release(true),
      pressed(false), released(true), hold(false), long_hold(false),
      has_new_press(false), has_new_release(false), has_new_hold_release(false),
      press_time(0), release_time(0),
      hold_threshold_ms(1000), long_hold_threshold_ms(10000), release_threshold_ms(100),
      touch_x(0), touch_y(0), gesture(GESTURE_NONE)
{
}

TouchPanel::~TouchPanel()
{
}

void TouchPanel::reset()
{
#if TP_RST_PIN >= 0
    if (rst_pin >= 0) {
        gpio_set_level((gpio_num_t)rst_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)rst_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGI(TAG, "Touch controller reset complete");
    }
#endif
}

esp_err_t TouchPanel::i2c_read_reg(uint8_t reg, uint8_t* data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    // Read data
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t TouchPanel::i2c_write_reg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t TouchPanel::initialize()
{
    ESP_LOGI(TAG, "Initializing CST816S touch controller...");

    // Reset the touch controller
    reset();

    // Try to read chip ID to verify communication
    uint8_t chip_id = getChipId();
    if (chip_id == 0 || chip_id == 0xFF) {
        ESP_LOGE(TAG, "Failed to communicate with CST816S (chip_id=0x%02X)", chip_id);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "CST816S chip ID: 0x%02X", chip_id);
    ESP_LOGI(TAG, "CST816S firmware version: 0x%02X", getFirmwareVersion());

    // Disable auto sleep
    i2c_write_reg(CST816S_REG_DIS_AUTO_SLEEP, 0x01);

    // Configure interrupt mode
    i2c_write_reg(CST816S_REG_IRQ_CTL, 0x41);  // Enable motion interrupt

    press_time = esp_timer_get_time() / 1000;
    initialized = true;

    ESP_LOGI(TAG, "CST816S initialization complete");
    return ESP_OK;
}

uint8_t TouchPanel::getChipId()
{
    uint8_t chip_id = 0;
    if (i2c_read_reg(CST816S_REG_CHIP_ID, &chip_id, 1) != ESP_OK) {
        return 0;
    }
    return chip_id;
}

uint8_t TouchPanel::getFirmwareVersion()
{
    uint8_t version = 0;
    if (i2c_read_reg(CST816S_REG_FW_VERSION, &version, 1) != ESP_OK) {
        return 0;
    }
    return version;
}

bool TouchPanel::readTouchData(touch_data_t* data)
{
    uint8_t buf[6];

    if (i2c_read_reg(CST816S_REG_GESTURE_ID, buf, 6) != ESP_OK) {
        data->available = false;
        return false;
    }

    data->gesture = (touch_gesture_t)buf[0];
    data->fingers = buf[1];
    data->x = ((buf[2] & 0x0F) << 8) | buf[3];
    data->y = ((buf[4] & 0x0F) << 8) | buf[5];
    data->available = (data->fingers > 0);

    return data->available;
}

void TouchPanel::handleTouchPanel()
{
    int64_t current_time = esp_timer_get_time() / 1000;  // Convert to milliseconds

    has_new_press = false;
    has_new_release = false;
    has_new_hold_release = false;

    touch_data_t data;
    bool touch_available = readTouchData(&data);

    if (touch_available) {
        if (!first_press) {
            touch_x = data.x;
            touch_y = data.y;
            gesture = data.gesture;

            if (!pressed) {
                has_new_press = true;
                pressed = true;
                released = false;
                press_time = current_time;
            } else if (!hold && (current_time - press_time > hold_threshold_ms)) {
                if (!long_hold && (current_time - press_time > long_hold_threshold_ms)) {
                    long_hold = true;
                }
                hold = true;
            }
        } else {
            first_press = false;
        }
    } else {
        if (pressed) {
            release_time = current_time;
        }

        if (!released && (current_time - release_time > release_threshold_ms)) {
            if (hold) {
                has_new_hold_release = true;
            }
            long_hold = false;
            hold = false;
            pressed = false;

            if (first_release) {
                first_release = false;
            } else {
                released = true;
                has_new_release = true;
            }
        }
    }
}
