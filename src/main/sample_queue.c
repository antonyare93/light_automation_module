#include <inttypes.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "sample_queue.h"

static const char *TAG = "queue";

typedef struct queue_node_t
{
    float data;
    queue_node_t *next;
} queue_node_t;

esp_err_t queue_create(queue_t **out_queue)
{
    if (out_queue == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    queue_t *q = malloc(sizeof(queue_t));
    if (q == NULL)
    {
        ESP_LOGE(TAG, "sin memoria para la cola (%" PRIu32 " B libres)", esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }

    q->front = NULL;
    q->rear = NULL;
    *out_queue = q;
    return ESP_OK;
}

static int queue_is_empty(queue_t *q)
{
    return q->front == NULL;
}

esp_err_t queue_enqueue(queue_t *q, float value)
{
    if (q == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* El malloc va primero: si falla, la cola no se ha tocado y se puede
     * devolver el error sin dejar nada a medio construir. */
    queue_node_t *new_node = malloc(sizeof(queue_node_t));
    if (new_node == NULL)
    {
        ESP_LOGE(TAG, "sin memoria para el nodo (%" PRIu32 " B libres)", esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }

    new_node->data = value;
    new_node->next = NULL;

    if (queue_is_empty(q))
    {
        q->front = new_node;
    }
    else
    {
        q->rear->next = new_node;
    }
    q->rear = new_node;
    return ESP_OK;
}

esp_err_t queue_dequeue(queue_t *q, float *out_value)
{
    if (q == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (queue_is_empty(q))
    {
        return ESP_ERR_INVALID_STATE;
    }

    queue_node_t *temp = q->front;

    if (out_value != NULL)
    {
        *out_value = temp->data;
    }

    q->front = temp->next;
    if (q->front == NULL)
    {
        q->rear = NULL;
    }

    free(temp);
    return ESP_OK;
}

esp_err_t queue_destroy(queue_t *q)
{
    if (q == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    while (!queue_is_empty(q))
    {
        queue_dequeue(q, NULL);
    }

    free(q);
    return ESP_OK;
}

esp_err_t queue_count(queue_t *q, int *out_count)
{
    if (q == NULL || out_count == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int cont = 0;
    if (queue_is_empty(q))
    {
        *out_count = cont;
        return ESP_OK;
    }

    queue_node_t *cursor_node = q->front;
    do
    {
        cont++;
        cursor_node = cursor_node->next;
    } while (cursor_node != NULL);

    *out_count = cont;
    return ESP_OK;
}

static float queue_sum(queue_t *q)
{
    float total = 0;
    if (queue_is_empty(q))
    {
        return total;
    }

    queue_node_t *cursor_node = q->front;
    do
    {
        total += cursor_node->data;
        cursor_node = cursor_node->next;
    } while (cursor_node != NULL);

    return total;
}

esp_err_t queue_get_stats(queue_t *q, queue_stats_t *out_stats)
{
    if (q == NULL || out_stats == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int count = 0;
    esp_err_t err = queue_count(q, &count);
    if (err != ESP_OK)
    {
        return err;
    }

    out_stats->count = count;
    out_stats->mean = 0.0f;
    if (count != 0)
    {
        out_stats->mean = queue_sum(q) / (float)count;
    }

    return ESP_OK;
}
