#include "hal_config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "HAL";

esp_err_t hal_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TP_SDA_PIN,
        .scl_io_num = TP_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C initialized on SDA=%d, SCL=%d", TP_SDA_PIN, TP_SCL_PIN);
    return ESP_OK;
}

esp_err_t hal_gpio_init(void)
{
    // Initialize touch panel interrupt pin
#if TP_INT_PIN >= 0
    gpio_config_t tp_int_conf = {
        .pin_bit_mask = (1ULL << TP_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&tp_int_conf);
    ESP_LOGI(TAG, "Touch INT pin %d configured", TP_INT_PIN);
#endif

    // Initialize touch panel reset pin
#if TP_RST_PIN >= 0
    gpio_config_t tp_rst_conf = {
        .pin_bit_mask = (1ULL << TP_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&tp_rst_conf);
    gpio_set_level(TP_RST_PIN, 1);
    ESP_LOGI(TAG, "Touch RST pin %d configured", TP_RST_PIN);
#endif

    // Initialize LCD backlight pin
#if LCD_BL_PIN >= 0
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << LCD_BL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl_conf);
    gpio_set_level(LCD_BL_PIN, 1);  // Turn on backlight
    ESP_LOGI(TAG, "LCD backlight pin %d configured", LCD_BL_PIN);
#endif

    // Initialize LCD reset pin
#if LCD_RST_PIN >= 0
    gpio_config_t lcd_rst_conf = {
        .pin_bit_mask = (1ULL << LCD_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&lcd_rst_conf);
    gpio_set_level(LCD_RST_PIN, 1);
    ESP_LOGI(TAG, "LCD RST pin %d configured", LCD_RST_PIN);
#endif

    return ESP_OK;
}

esp_err_t hal_init(void)
{
    ESP_LOGI(TAG, "Initializing HAL...");

#if CONFIG_IDF_TARGET_ESP32S3
    ESP_LOGI(TAG, "Target: ESP32-S3 with GC9A01 LCD (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
#elif CONFIG_IDF_TARGET_ESP32C6
    ESP_LOGI(TAG, "Target: ESP32-C6 with RM67162 AMOLED (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif

    esp_err_t err = hal_gpio_init();
    if (err != ESP_OK) {
        return err;
    }

    err = hal_i2c_init();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "HAL initialization complete");
    return ESP_OK;
}
