#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Bus I2C donde cuelga el sensor. */
#define VEML7700_SDA_GPIO    6
#define VEML7700_SCL_GPIO    7
#define VEML7700_I2C_FREQ_HZ 100000

/* Crea el bus I2C, comprueba que el sensor responde, lo registra y lo saca
 * de shutdown. Devuelve ESP_ERR_NOT_FOUND si no hay nadie en 0x10. */
esp_err_t veml7700_init(void);

/* Lee el canal ALS y lo convierte a lux. raw puede ser NULL. */
esp_err_t veml7700_read_lux(uint16_t *raw, float *lux);

/* Lux por cuenta con la ganancia y el tiempo de integracion configurados. */
float veml7700_resolution(void);

/* El dato crudo es de 16 bits: si llega al tope, la medida ya no es fiable. */
bool veml7700_is_saturated(uint16_t raw);

esp_err_t veml7700_shutdown(void);

esp_err_t veml7700_log_diagnostics(void);
