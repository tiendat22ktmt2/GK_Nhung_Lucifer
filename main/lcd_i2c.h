#pragma once
/* lcd_i2c.h - LCD 16x2 qua PCF8574 (I2C) */

#include <stdint.h>
#include "esp_err.h"

esp_err_t lcd_init(void);
void lcd_clear(void);
void lcd_print_line(uint8_t row, const char *text);   /* toi da 16 ky tu, tu xoa phan du */