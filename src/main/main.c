#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "deep_sleep.h"
#include "led_pwm.h"
#include "veml7700.h"
#include "sample_queue.h"

static const char *TAG = "example";

#define SAMPLE_PERIOD_US 100000
#define SAMPLE_WINDOW 10
#define SAMPLE_MAX_CONSECUTIVE_FAILURES 10

#define LED_STAYS_OFF false
#define LED_STAYS_ON true

static bool light_fell_below_low_cut(float mean)
{
    return mean < LED_PWM_MIN_LUX_OFF;
}

static bool light_rose_above_high_cut(float mean)
{
    return mean > LED_PWM_MAX_LUX_OFF;
}

static bool light_recovered_from_dark(float mean)
{
    return mean > LED_PWM_MIN_LUX_ON;
}

static bool light_recovered_from_glare(float mean)
{
    return mean < LED_PWM_MAX_LUX_ON;
}

static esp_err_t fill_sample_window(queue_t *q, queue_stats_t *stats, uint16_t *raw, float *lux)
{
    int consecutive_failures = 0;

    while (stats->count < SAMPLE_WINDOW)
    {
        esp_err_t err = veml7700_read_lux(raw, lux);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "lectura fallida: %s", esp_err_to_name(err));

            if (++consecutive_failures >= SAMPLE_MAX_CONSECUTIVE_FAILURES)
            {
                return err;
            }

            usleep(SAMPLE_PERIOD_US);
            continue;
        }

        consecutive_failures = 0;
        queue_enqueue(q, *lux);
        queue_get_stats(q, stats);

        if (stats->count < SAMPLE_WINDOW)
        {
            usleep(SAMPLE_PERIOD_US);
        }
    }

    return ESP_OK;
}

static void sleep_until_next_cycle(bool led_stays_on)
{
    ESP_ERROR_CHECK(deep_sleep_register_rtc_timer_wakeup(DEEP_SLEEP_DEFAULT_MINUTES));
    deep_sleep_enter(led_stays_on);
}

void app_main(void)
{
    deep_sleep_release_pad_hold();
    deep_sleep_report_wakeup();

    ESP_ERROR_CHECK(led_pwm_init());

    if (veml7700_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "VEML7700 no responde; durmiendo hasta el siguiente ciclo");
        sleep_until_next_cycle(led_pwm_sleep_level());
    }

    queue_t *q = NULL;
    queue_create(&q);
    queue_stats_t stats;
    queue_get_stats(q, &stats);

    ESP_LOGI(TAG, "VEML7700 listo: %.4f lx/cuenta", veml7700_resolution());
    uint16_t raw;
    float lux;

    if (fill_sample_window(q, &stats, &raw, &lux) != ESP_OK)
    {
        ESP_LOGE(TAG, "muestreo abortado; durmiendo hasta el siguiente ciclo");
        sleep_until_next_cycle(led_pwm_sleep_level());
    }

    veml7700_log_diagnostics();
    ESP_LOGI(TAG, "media tras llenado: %.1f lx (%d muestras)", stats.mean, stats.count);

    if (!light_recovered_from_dark(stats.mean))
    {
        sleep_until_next_cycle(LED_STAYS_OFF);
    }

    if (!light_recovered_from_glare(stats.mean))
    {
        sleep_until_next_cycle(LED_STAYS_ON);
    }

    while (1)
    {
        int duty = led_pwm_set_from_lux(stats.mean);

        ESP_LOGI(TAG, "Raw: %u; Lux: %.1f%s; Duty: %d; Mean: %.1f", raw, lux, veml7700_is_saturated(raw) ? " (SATURADO)" : "", duty, stats.mean);
        queue_dequeue(q, NULL);
        queue_get_stats(q, &stats);

        if (fill_sample_window(q, &stats, &raw, &lux) != ESP_OK)
        {
            ESP_LOGE(TAG, "muestreo abortado; durmiendo hasta el siguiente ciclo");
            sleep_until_next_cycle(led_pwm_sleep_level());
        }

        usleep(SAMPLE_PERIOD_US);

        if (light_fell_below_low_cut(stats.mean))
        {
            sleep_until_next_cycle(LED_STAYS_OFF);
        }

        if (light_rose_above_high_cut(stats.mean))
        {
            sleep_until_next_cycle(LED_STAYS_ON);
        }
    }
}
