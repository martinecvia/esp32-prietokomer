#pragma once

// SPI
#define PIN_SD_CS 5
#define PIN_SD_SCK 18
#define PIN_SD_MISO 19
#define PIN_SD_MOSI 23
#define SPI_FREQUENCY 4000000
// I2C
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_FREQUENCY 400000
#define __ADDR_I2C_AT24C32 0x57
#define __ADDR_I2C_DS3231 0x68
#define __ADDR_I2C_SSD1306 0x3C
#define __ADDR_I2C_BME280 0x76
// Cyble NF-1
#define PIN_CYBLE_NF1 4
#define PIN_BUTTON_3_CYBLE_NF1 25
#define CYBLE_NF1_DEBOUNCE_MS 15
// Buttons
#define PIN_BUTTON_1 27
#define PIN_BUTTON_2 26
#define BUTTON_DEBOUNCE_MS 30
// LED
#define PIN_LED_R 32
#define PIN_LED_Y 13
#define PIN_LED_G 33
// WiFi
#define WIFI_AP_SSID "sd_pulse_logger"
#define WIFI_AP_PASSWORD "slocum2011"