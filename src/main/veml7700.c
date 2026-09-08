#include <unistd.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "veml7700.h"

static const char *TAG = "veml7700";

#define I2C_TIMEOUT_MS 100

#define VEML7700_ADDR      0x10
#define VEML7700_REG_CONF  0x00
#define VEML7700_REG_ALS   0x04
#define VEML7700_REG_WHITE 0x05

/* Ganancia x1, tiempo de integracion 100 ms.
 * Los bits van al registro y el valor fisico entra en el calculo de lux:
 * si cambias uno hay que cambiar el otro o los lux salen mal. */
#define VEML7700_GAIN_BITS  0x0 /* 00   -> x1     */
#define VEML7700_GAIN_VALUE 1.0f
#define VEML7700_IT_BITS    0x0 /* 0000 -> 100 ms */
#define VEML7700_IT_MS      100.0f

#define VEML7700_CONF_ALS_SD_BIT 0x0001
#define VEML7700_CONF_POWER_ON   (((uint16_t)VEML7700_GAIN_BITS << 11) | ((uint16_t)VEML7700_IT_BITS << 6))
#define VEML7700_CONF_SHUTDOWN   (VEML7700_CONF_POWER_ON | VEML7700_CONF_ALS_SD_BIT)

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t veml_write(uint8_t reg, uint16_t val)
{
    /* Registros de 16 bits en little-endian: primero el byte bajo. */
    uint8_t buf[3] = {reg, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8)};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t veml_read(uint8_t reg, uint16_t *out)
{
    /* transmit_receive hace el RESTART sin soltar el bus. Con un transmit y un
     * receive por separado iria un STOP en medio y el sensor perderia el
     * puntero de registro. */
    uint8_t raw[2];
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, raw, sizeof(raw), I2C_TIMEOUT_MS);
    if (err != ESP_OK)
    {
        return err;
    }

    *out = (uint16_t)(raw[0] | (raw[1] << 8));
    return ESP_OK;
}

esp_err_t veml7700_init(void)
{
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = VEML7700_SDA_GPIO,
        .scl_io_num = VEML7700_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };

    esp_err_t err = i2c_new_master_bus(&i2c_config, &s_bus);
    if (err != ESP_OK)
    {
        return err;
    }

    err = i2c_master_probe(s_bus, VEML7700_ADDR, I2C_TIMEOUT_MS);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "no hay nadie en 0x%02X; revisa SDA=%d, SCL=%d, 3V3 y GND",
                 VEML7700_ADDR, VEML7700_SDA_GPIO, VEML7700_SCL_GPIO);
        return err;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = VEML7700_ADDR,
        .scl_speed_hz = VEML7700_I2C_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(s_bus, &dev_config, &s_dev);
    if (err != ESP_OK)
    {
        return err;
    }

    /* Con ganancia x1, IT 100 ms y sin interrupciones la palabra queda en
     * 0x0000, pero hay que escribirla igual: el valor de reset tiene
     * ALS_SD = 1 y el sensor arranca apagado. */
    err = veml_write(VEML7700_REG_CONF, VEML7700_CONF_POWER_ON);
    if (err != ESP_OK)
    {
        return err;
    }

    /* 2.5 ms de arranque + un tiempo de integracion para tener una medida
     * completa. Leer antes devuelve 0. */
    usleep(3000 + (int)VEML7700_IT_MS * 1000);
    return ESP_OK;
}

float veml7700_resolution(void)
{
    /* 0.0036 lx/cuenta es el caso de referencia del datasheet (x2, 800 ms);
     * escala de forma inversa con ganancia y tiempo de integracion. */
    return 0.0036f * (2.0f / VEML7700_GAIN_VALUE) * (800.0f / VEML7700_IT_MS);
}

bool veml7700_is_saturated(uint16_t raw)
{
    return raw == UINT16_MAX;
}

esp_err_t veml7700_shutdown(void)
{
    if (s_dev == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return veml_write(VEML7700_REG_CONF, VEML7700_CONF_SHUTDOWN);
}

esp_err_t veml7700_log_diagnostics(void)
{
    if (s_dev == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t conf = 0;
    uint16_t als = 0;
    uint16_t white = 0;

    esp_err_t err = veml_read(VEML7700_REG_CONF, &conf);
    if (err != ESP_OK)
    {
        return err;
    }

    err = veml_read(VEML7700_REG_ALS, &als);
    if (err != ESP_OK)
    {
        return err;
    }

    err = veml_read(VEML7700_REG_WHITE, &white);
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "diag conf=0x%04X expected=0x%04X als=%u white=%u res=%.4f",
             conf, VEML7700_CONF_POWER_ON, als, white, veml7700_resolution());

    return ESP_OK;
}

esp_err_t veml7700_read_lux(uint16_t *raw, float *lux)
{
    uint16_t counts;

    esp_err_t err = veml_read(VEML7700_REG_ALS, &counts);
    if (err != ESP_OK)
    {
        return err;
    }

    float value = (float)counts * veml7700_resolution();

    /* Correccion de no linealidad de la nota de aplicacion de Vishay. */
    if (value > 1000.0f)
    {
        value = 6.0135e-13f * value * value * value * value
              - 9.3924e-9f * value * value * value
              + 8.1488e-5f * value * value
              + 1.0023f * value;
    }

    if (raw != NULL)
    {
        *raw = counts;
    }
    *lux = value;
    return ESP_OK;
}
