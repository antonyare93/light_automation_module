#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_serial_output.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "deep_sleep.h"
#include "led_pwm.h"
#include "veml7700.h"

static const char *TAG = "deep_sleep";

#define SECONDS_PER_MINUTE 60ULL
#define MICROSECONDS_PER_SECOND 1000000ULL
#define MICROSECONDS_PER_MILLISECOND 1000

RTC_DATA_ATTR static struct timeval sleep_enter_time;

static bool is_cold_boot(uint32_t wakeup_causes)
{
    return (wakeup_causes & BIT(ESP_SLEEP_WAKEUP_UNDEFINED)) != 0;
}

static bool woke_from_timer(uint32_t wakeup_causes)
{
    return (wakeup_causes & BIT(ESP_SLEEP_WAKEUP_TIMER)) != 0;
}

static int64_t elapsed_ms_since_sleep_enter(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    return (int64_t)(now.tv_sec - sleep_enter_time.tv_sec) * 1000
         + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;
}

static int64_t awake_ms(void)
{
    return esp_timer_get_time() / MICROSECONDS_PER_MILLISECOND;
}

static void hold_minimum_awake_window(void)
{
    int64_t elapsed_ms = awake_ms();

    if (elapsed_ms >= DEEP_SLEEP_AWAKE_WINDOW_MS)
    {
        return;
    }

    usleep((useconds_t)((DEEP_SLEEP_AWAKE_WINDOW_MS - elapsed_ms) * MICROSECONDS_PER_MILLISECOND));
}

static void wait_until_console_is_flushed(void)
{
    fflush(stdout);
#ifdef CONFIG_ESP_CONSOLE_UART_NUM
    esp_rom_output_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
#endif
}

void deep_sleep_release_pad_hold(void)
{
    gpio_force_unhold_all();
    gpio_deep_sleep_hold_dis();
}

void deep_sleep_report_wakeup(void)
{
    uint32_t wakeup_causes = esp_sleep_get_wakeup_causes();

    if (is_cold_boot(wakeup_causes))
    {
        ESP_LOGI(TAG, "cold boot, not woken from deep sleep");
        return;
    }

    int64_t slept_ms = elapsed_ms_since_sleep_enter();

    if (woke_from_timer(wakeup_causes))
    {
        ESP_LOGI(TAG, "timer wakeup after %" PRId64 " ms asleep", slept_ms);
    }
    else
    {
        ESP_LOGI(TAG, "wakeup causes 0x%08" PRIx32 " after %" PRId64 " ms asleep",
                 wakeup_causes, slept_ms);
    }
}

esp_err_t deep_sleep_register_rtc_timer_wakeup(int minutes)
{
    if (minutes <= 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t wakeup_time_us = (uint64_t)minutes * SECONDS_PER_MINUTE * MICROSECONDS_PER_SECOND;

    esp_err_t err = esp_sleep_enable_timer_wakeup(wakeup_time_us);
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "timer wakeup armed for %d min", minutes);
    return ESP_OK;
}

void deep_sleep_enter(bool led_stays_on)
{
    hold_minimum_awake_window();

    esp_err_t err = veml7700_shutdown();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "veml7700_shutdown failed: %s", esp_err_to_name(err));
    }

    err = led_pwm_latch_for_sleep(led_stays_on);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "led_pwm_latch_for_sleep failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "entering deep sleep after %" PRId64 " ms awake, led %s",
             awake_ms(), led_stays_on ? "on" : "off");
    wait_until_console_is_flushed();

    gettimeofday(&sleep_enter_time, NULL);
    esp_deep_sleep_start();
}
