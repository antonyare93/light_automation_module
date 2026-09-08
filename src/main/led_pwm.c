#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "led_pwm.h"

#define LED_PWM_SPEED_MODE LEDC_LOW_SPEED_MODE
#define LED_PWM_TIMER LEDC_TIMER_0
#define LED_PWM_CHANNEL LEDC_CHANNEL_0

#define LED_PWM_LEVEL_ON 1
#define LED_PWM_LEVEL_OFF 0

RTC_DATA_ATTR static bool s_led_on_across_sleep;

static int duty_from_lux(float lux)
{
    if (lux < LED_PWM_MIN_LUX_OFF)
    {
        return 0;
    }

    /* Normalizamos en float y aplicamos la curva antes de pasar a
     * entero: haciendolo al reves la truncacion se come la zona baja. */
    float x = (lux - LED_PWM_MIN_LUX_OFF) / (LED_PWM_MAX_LUX_OFF - LED_PWM_MIN_LUX_OFF);

    if (x > 1.0f)
    {
        x = 1.0f;
    }

    /* Curva cuadratica: compensa que el ojo no percibe el brillo de
     * forma lineal. El recorrido va de MIN_DUTY a MAX_DUTY, asi que
     * en cuanto se pasa MIN_LUX el LED ya enciende. */
    return LED_PWM_MIN_DUTY + (int)(x * x * (float)(LED_PWM_MAX_DUTY - LED_PWM_MIN_DUTY));
}

static esp_err_t configure_ledc(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LED_PWM_SPEED_MODE,
        .timer_num = LED_PWM_TIMER,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = LED_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK)
    {
        return err;
    }

    ledc_channel_config_t channel = {
        .speed_mode = LED_PWM_SPEED_MODE,
        .channel = LED_PWM_CHANNEL,
        .timer_sel = LED_PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_PWM_GPIO,
        .duty = s_led_on_across_sleep ? LED_PWM_MAX_DUTY : 0,
        .hpoint = 0,
    };

    return ledc_channel_config(&channel);
}

esp_err_t led_pwm_init(void)
{
    esp_err_t ledc_err = configure_ledc();

    esp_err_t hold_err = gpio_hold_dis(LED_PWM_GPIO);

    return ledc_err != ESP_OK ? ledc_err : hold_err;
}

esp_err_t led_pwm_set_duty(int duty)
{
    if (duty > LED_PWM_MAX_DUTY)
    {
        duty = LED_PWM_MAX_DUTY;
    }
    else if (duty < 0)
    {
        duty = 0;
    }

    esp_err_t err = ledc_set_duty(LED_PWM_SPEED_MODE, LED_PWM_CHANNEL, duty);
    if (err != ESP_OK)
    {
        return err;
    }

    return ledc_update_duty(LED_PWM_SPEED_MODE, LED_PWM_CHANNEL);
}

int led_pwm_set_from_lux(float lux)
{
    int duty = duty_from_lux(lux);

    led_pwm_set_duty(duty);
    return duty;
}

esp_err_t led_pwm_latch_for_sleep(bool stay_on)
{
    uint32_t idle_level = stay_on ? LED_PWM_LEVEL_ON : LED_PWM_LEVEL_OFF;

    esp_err_t err = ledc_stop(LED_PWM_SPEED_MODE, LED_PWM_CHANNEL, idle_level);
    if (err != ESP_OK)
    {
        return err;
    }

    err = gpio_hold_en(LED_PWM_GPIO);
    if (err != ESP_OK)
    {
        return err;
    }

    gpio_deep_sleep_hold_en();

    s_led_on_across_sleep = stay_on;
    return ESP_OK;
}

bool led_pwm_sleep_level(void)
{
    return s_led_on_across_sleep;
}
