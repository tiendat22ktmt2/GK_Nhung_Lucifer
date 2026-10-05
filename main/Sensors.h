#pragma once
/* sensors.h - DHT22 (one-wire) va LDR (ADC) */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

void dht22_init(void);
bool dht22_read(float *temperature, float *humidity);   /* >= 2 s giua 2 lan doc */

esp_err_t ldr_init(void);
esp_err_t ldr_read(uint16_t *raw, uint8_t *percent);    /* percent: 100 = sang nhat */
const char *ldr_level_str(uint8_t percent);             /* None/Weak/Medium/Bright */