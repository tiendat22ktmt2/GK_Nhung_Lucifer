idf_component_register(
    SRCS "main.c" "Sensors.c" "lcd_i2c.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c esp_adc esp_timer
)