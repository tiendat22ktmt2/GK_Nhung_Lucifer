#include "sensors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

/* ---------------- Cau hinh ---------------- */
#define DHT_GPIO          GPIO_NUM_4
#define LDR_ADC_CHANNEL   ADC_CHANNEL_3          /* GPIO3 */

#define LIGHT_NONE_MAX    40                     /* < 40 : None   */
#define LIGHT_WEAK_MAX    65                     /* < 65 : Weak   */
#define LIGHT_MEDIUM_MAX  75                     /* < 75 : Medium, con lai Bright */

/* ---------------- DHT22 ---------------- */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* Cho trong luc chan con o muc `level`. Tra ve so us, hoac -1 neu timeout. */
int wait_while_level(int level, int timeout_us)
{
    int64_t start = esp_timer_get_time();

    while (gpio_get_level(DHT_GPIO) == level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

void dht22_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << DHT_GPIO,
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    gpio_set_level(DHT_GPIO, 1);
}

bool dht22_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};
    bool ok = true;

    /* Start signal */
    gpio_set_level(DHT_GPIO, 0);
    esp_rom_delay_us(2000);
    gpio_set_level(DHT_GPIO, 1);
    esp_rom_delay_us(30);

    /* 40 bit can timing chinh xac -> tat ngat */
    portENTER_CRITICAL(&s_mux);
    do {
        if (wait_while_level(1, 100) < 0) { ok = false; break; }
        if (wait_while_level(0, 100) < 0) { ok = false; break; }
        if (wait_while_level(1, 100) < 0) { ok = false; break; }

        for (int i = 0; i < 40; i++) {
            if (wait_while_level(0, 100) < 0) { ok = false; break; }

            int t = wait_while_level(1, 100);       /* 26 us = 0, 70 us = 1 */
            if (t < 0) { ok = false; break; }

            data[i / 8] <<= 1;
            if (t > 40) {
                data[i / 8] |= 1;
            }
        }
    } while (0);
    portEXIT_CRITICAL(&s_mux);

    if (!ok || ((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]) {
        return false;
    }

    *humidity = ((data[0] << 8) | data[1]) / 10.0f;

    float t = (((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    *temperature = (data[2] & 0x80) ? -t : t;

    return true;
}

/* ---------------- LDR ---------------- */
static adc_oneshot_unit_handle_t s_adc;

esp_err_t ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(s_adc, LDR_ADC_CHANNEL, &ch_cfg);
}

esp_err_t ldr_read(uint16_t *raw, uint8_t *percent)
{
    int value = 0;
    esp_err_t err = adc_oneshot_read(s_adc, LDR_ADC_CHANNEL, &value);
    if (err != ESP_OK) {
        return err;
    }

    *raw     = (uint16_t)value;
    *percent = (uint8_t)(100 - (value * 100) / 4095);   /* dao chieu neu module nguoc */
    return ESP_OK;
}

const char *ldr_level_str(uint8_t percent)
{
    if (percent < LIGHT_NONE_MAX)   return "None";
    if (percent < LIGHT_WEAK_MAX)   return "Weak";
    if (percent < LIGHT_MEDIUM_MAX) return "Medium";
    return "Bright";
}