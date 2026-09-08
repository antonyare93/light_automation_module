idf_component_register(SRCS "main.c" "veml7700.c" "led_pwm.c" "sample_queue.c" "deep_sleep.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_timer)
