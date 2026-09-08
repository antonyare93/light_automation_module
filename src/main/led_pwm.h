#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define LED_PWM_GPIO 10
#define LED_PWM_FREQ_HZ 5000

/* 13 bits de resolucion -> duty de 0 a 8191. Es el maximo que permite el
 * LEDC a 5 kHz (5000 * 8192 = 41 MHz, cabe en los 80 MHz del APB). */
#define LED_PWM_MAX_DUTY 8191

/* Duty con el que arranca justo en LED_PWM_MIN_LUX. Sin este suelo la
 * cuadratica empieza en 0 y el LED no enciende hasta bien entrada la escala. */
#define LED_PWM_MIN_DUTY 0

/* Rango de luz ambiente que se mapea al brillo. Ajustalo con los lux que
 * midas en el sitio real: por debajo de MIN_LUX el LED se apaga, en MIN_LUX
 * arranca con MIN_DUTY y en MAX_LUX llega a MAX_DUTY. Toda la escala se
 * mueve con estos dos valores. */
#define LED_PWM_MIN_LUX_OFF 10.0f
#define LED_PWM_MIN_LUX_ON 15.0f
#define LED_PWM_MAX_LUX_OFF 190.0f
#define LED_PWM_MAX_LUX_ON 185.0f

/* Configura el timer y el canal del LEDC. */
esp_err_t led_pwm_init(void);

/* Aplica un duty crudo (0 .. LED_PWM_MAX_DUTY). */
esp_err_t led_pwm_set_duty(int duty);

/* Mapea los lux al duty segun la escala de arriba y lo aplica.
 * Devuelve el duty que quedo puesto, util para el log. */
int led_pwm_set_from_lux(float lux);

esp_err_t led_pwm_latch_for_sleep(bool stay_on);

bool led_pwm_sleep_level(void);
