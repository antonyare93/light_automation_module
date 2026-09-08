#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define DEEP_SLEEP_DEFAULT_MINUTES 5
#define DEEP_SLEEP_AWAKE_WINDOW_MS 3000

void deep_sleep_release_pad_hold(void);

void deep_sleep_report_wakeup(void);

esp_err_t deep_sleep_register_rtc_timer_wakeup(int minutes);

void deep_sleep_enter(bool led_stays_on) __attribute__((__noreturn__));
