#include "lcd_i2c.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

/* ---------------- Cau hinh ---------------- */
#define I2C_SDA_GPIO      GPIO_NUM_5
#define I2C_SCL_GPIO      GPIO_NUM_6
#define LCD_ADDR          0x27
#define LCD_ADDR_ALT      0x3F

/* PCF8574: P0=RS P1=RW P2=EN P3=BL P4..P7=D4..D7 */
#define LCD_RS  0x01
#define LCD_EN  0x04
#define LCD_BL  0x08

static const char *TAG = "LCD";
static i2c_master_dev_handle_t s_dev;

/* ---------------- Muc thap ----------------
 * Bo static -> doi ten co tien to lcd_ de khong trung ten symbol
 * (dac biet "send" trung voi send() cua lwip).
 */
void lcd_expander_write(uint8_t data)
{
    uint8_t b = data | LCD_BL;
    esp_err_t err = i2c_master_transmit(s_dev, &b, 1, 100);

    if (err != ESP_OK) {
        static bool logged = false;
        if (!logged) {
            ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(err));
            logged = true;
        }
    }
}

/* Ghi DATA truoc, sau do moi xung EN (giong thu vien Arduino) */
void lcd_write4bits(uint8_t value)
{
    lcd_expander_write(value);
    lcd_expander_write(value | LCD_EN);
    esp_rom_delay_us(1);
    lcd_expander_write(value & ~LCD_EN);
    esp_rom_delay_us(50);
}

void lcd_send(uint8_t value, uint8_t mode)
{
    lcd_write4bits((value & 0xF0) | mode);
    lcd_write4bits(((uint8_t)(value << 4) & 0xF0) | mode);
}

void lcd_cmd(uint8_t c)
{
    lcd_send(c, 0);
}

/* ---------------- API ---------------- */
esp_err_t lcd_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = I2C_SDA_GPIO,
        .scl_io_num        = I2C_SCL_GPIO,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t addr = LCD_ADDR;
    if (i2c_master_probe(bus, LCD_ADDR, 50) != ESP_OK) {
        if (i2c_master_probe(bus, LCD_ADDR_ALT, 50) != ESP_OK) {
            ESP_LOGE(TAG, "LCD not found at 0x%02X or 0x%02X", LCD_ADDR, LCD_ADDR_ALT);
            return ESP_ERR_NOT_FOUND;
        }
        addr = LCD_ADDR_ALT;
    }
    ESP_LOGI(TAG, "LCD found at 0x%02X", addr);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 100000,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    /* Khoi tao giong thu vien Arduino LiquidCrystal_I2C */
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_expander_write(0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    lcd_write4bits(0x30);
    esp_rom_delay_us(4500);
    lcd_write4bits(0x30);
    esp_rom_delay_us(4500);
    lcd_write4bits(0x30);
    esp_rom_delay_us(150);
    lcd_write4bits(0x20);           /* 4-bit mode */

    lcd_cmd(0x28);                  /* 2 dong, font 5x8 */
    lcd_cmd(0x0C);                  /* display ON, cursor OFF */
    lcd_clear();
    lcd_cmd(0x06);                  /* entry mode */

    return ESP_OK;
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
    esp_rom_delay_us(2000);
}

void lcd_print_line(uint8_t row, const char *text)
{
    int i = 0;

    lcd_cmd(0x80 | (row ? 0x40 : 0x00));

    for (; i < 16 && text[i]; i++) {
        lcd_send((uint8_t)text[i], LCD_RS);
    }
    for (; i < 16; i++) {
        lcd_send(' ', LCD_RS);
    }
}