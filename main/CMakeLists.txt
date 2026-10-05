idf_component_register(
    SRCS "main.c" "sensors.c" "lcd_i2c.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc esp_timer
)