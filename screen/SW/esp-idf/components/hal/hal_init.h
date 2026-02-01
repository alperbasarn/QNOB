#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Hardware Abstraction Layer
 *
 * This function initializes all hardware-related configurations including:
 * - GPIO pins for display, touch, and other peripherals
 * - I2C bus for touch panel and IMU
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hal_init(void);

/**
 * @brief Initialize I2C master bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hal_i2c_init(void);

/**
 * @brief Initialize GPIO pins
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hal_gpio_init(void);

#ifdef __cplusplus
}
#endif
