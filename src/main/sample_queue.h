#pragma once

#include "esp_err.h"

typedef struct queue_node_t queue_node_t;

typedef struct queue_t
{
    queue_node_t *front;
    queue_node_t *rear;
} queue_t;

typedef struct queue_stats_t
{
    int count;
    float mean;
} queue_stats_t;

/* Crea la cola y deja el puntero en *out_queue.
 * Liberala despues con queue_destroy().
 *   ESP_ERR_INVALID_ARG  out_queue es NULL
 *   ESP_ERR_NO_MEM       no hay memoria para la cola */
esp_err_t queue_create(queue_t **out_queue);

/* Encola un valor al final.
 *   ESP_ERR_INVALID_ARG  q es NULL
 *   ESP_ERR_NO_MEM       no hay memoria para el nodo; la cola queda intacta */
esp_err_t queue_enqueue(queue_t *q, float value);

/* Saca el valor mas antiguo y lo deja en *out_value.
 * out_value puede ser NULL si solo quieres descartar el elemento.
 *   ESP_ERR_INVALID_ARG    q es NULL
 *   ESP_ERR_INVALID_STATE  la cola esta vacia */
esp_err_t queue_dequeue(queue_t *q, float *out_value);

/* Libera todos los nodos y la propia cola.
 *   ESP_ERR_INVALID_ARG  q es NULL */
esp_err_t queue_destroy(queue_t *q);

/* Deja el numero de elementos en *out_count.
 *   ESP_ERR_INVALID_ARG  q o out_count son NULL */
esp_err_t queue_count(queue_t *q, int *out_count);

/* Deja la cuenta y el promedio en *out_stats.
 * Con la cola vacia devuelve count = 0 y mean = 0.
 *   ESP_ERR_INVALID_ARG  q o out_stats son NULL */
esp_err_t queue_get_stats(queue_t *q, queue_stats_t *out_stats);
