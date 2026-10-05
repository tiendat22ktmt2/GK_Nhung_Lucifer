/*
 * Midterm FreeRTOS - ESP32-C3 (ESP-IDF >= 5.2)
 * DHT22 (GPIO4) + LDR (GPIO3, ADC) + LCD 16x2 I2C (SDA GPIO5, SCL GPIO6)
 *
 *   sensors.c/.h : driver DHT22 + LDR
 *   lcd_i2c.c/.h : driver LCD
 *   main.c       : struct, queue, queue set, 3 task, app_main
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "sensors.h"
#include "lcd_i2c.h"

static const char *TAG = "APP";

/* ---------------- Cau hinh ---------------- */
#define DHT_PERIOD_MS     2000                   /* DHT22 >= 2 s */
#define LIGHT_PERIOD_MS   500
#define DHT_Q_LEN         5
#define LIGHT_Q_LEN       5

/* ---------------- Struct ---------------- */
typedef struct {
    float    temperature;
    float    humidity;
    uint32_t tick;
} dht_data_t;

typedef struct {
    uint16_t raw;           /* 0..4095 */
    uint8_t  percent;       /* 0..100  */
    uint32_t tick;
} light_data_t;

static QueueHandle_t    dht_queue;
static QueueHandle_t    light_queue;
static QueueSetHandle_t sensor_set;

/* ---------------- Tasks ---------------- */
void dht_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    dht_data_t d;

    for (;;) {
        float t, h;

        if (dht22_read(&t, &h)) {
            d.temperature = t;
            d.humidity    = h;
            d.tick        = xTaskGetTickCount();
            if (xQueueSend(dht_queue, &d, 0) != pdPASS) {
                ESP_LOGW(TAG, "dht_queue full");
            }
        } else {
            ESP_LOGW(TAG, "DHT read failed");
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DHT_PERIOD_MS));
    }
}

void light_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    light_data_t d;

    for (;;) {
        if (ldr_read(&d.raw, &d.percent) == ESP_OK) {
            d.tick = xTaskGetTickCount();
            if (xQueueSend(light_queue, &d, 0) != pdPASS) {
                ESP_LOGW(TAG, "light_queue full");
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LIGHT_PERIOD_MS));
    }
}

void lcd_task(void *arg)
{
    dht_data_t   dht_d   = {0};
    light_data_t light_d = {0};
    char line[96];                      /* lcd_print_line tu cat con 16 ky tu */

    lcd_print_line(0, "FreeRTOS ESP32C3");
    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd_clear();

    for (;;) {
        /* Block cho den khi 1 trong 2 queue co du lieu */
        QueueSetMemberHandle_t member = xQueueSelectFromSet(sensor_set, portMAX_DELAY);

        if (member == dht_queue) {
            xQueueReceive(dht_queue, &dht_d, 0);
            ESP_LOGI(TAG, "DHT: T=%.1f C, H=%.1f %%", dht_d.temperature, dht_d.humidity);
        } else if (member == light_queue) {
            xQueueReceive(light_queue, &light_d, 0);
            ESP_LOGI(TAG, "LDR: raw=%u (%u%%)", light_d.raw, light_d.percent);
        }

        snprintf(line, sizeof(line), "T:%4.1fC H:%3.0f%%", dht_d.temperature, dht_d.humidity);
        lcd_print_line(0, line);

        snprintf(line, sizeof(line), "Light: %s", ldr_level_str(light_d.percent));
        lcd_print_line(1, line);
    }
}

/* ---------------- app_main ---------------- */
void app_main(void)
{
    dht22_init();
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(ldr_init());

    dht_queue   = xQueueCreate(DHT_Q_LEN,   sizeof(dht_data_t));
    light_queue = xQueueCreate(LIGHT_Q_LEN, sizeof(light_data_t));
    sensor_set  = xQueueCreateSet(DHT_Q_LEN + LIGHT_Q_LEN);     /* = tong do dai cac queue */
    configASSERT(dht_queue && light_queue && sensor_set);

    xQueueAddToSet(dht_queue,   sensor_set);                    /* queue phai rong luc them */
    xQueueAddToSet(light_queue, sensor_set);

    xTaskCreate(dht_task,   "dht_task",   3072, NULL, 5, NULL);
    xTaskCreate(light_task, "light_task", 3072, NULL, 5, NULL);
    xTaskCreate(lcd_task,   "lcd_task",   4096, NULL, 4, NULL);
}