#include <Wire.h>
#include <Arduino.h>

#include "cfg.h"
#include "modules/led.h"
#include "modules/lcd.h"
#include "modules/rtc.h"
#include "modules/bme.h"
#include "modules/sdw.h"

Led led;
Lcd<Adafruit_SH1106G> lcd(128, 64);
Rtc rtc;
Bme bme;
Sdw sdw;

struct __lcd_display_state
{
    // Emvironment
    float t = 0.0f;

    // Clock
    uint8_t h = 0, m = 0, s = 0;
    uint8_t D = 0, M = 0;

    float flow = 0.0f;
    uint32_t pulseCount = 0;

    // SD
    float sd_used = 0.0f;
    float sd_size = 0.0f;
};
__lcd_display_state s;

void setup()
{
    Serial.begin(115200); // Serial
    delay(300);           // 0.3s delay

    Serial.println("\n\n\nsd_pulse_logger");
    if (!led.begin(PIN_LED_R, PIN_LED_Y, PIN_LED_G))
    {
        Serial.println("! LED init failed");
    }
    led.R();

    // I2C
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQUENCY);
    if (!lcd.begin(__ADDR_I2C_SSD1306))
    {
        Serial.println("! LCD init failed");
    }
    lcd.clear();

    if (!bme.begin(__ADDR_I2C_BME280))
    {
        Serial.println("! BME init failed");
    }

    // Required
    if (!rtc.begin(__ADDR_I2C_AT24C32, __ADDR_I2C_DS3231))
    {
        Serial.println("RTC init failed");
        led.G();
        return;
    }
    if (!sdw.begin(PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, true, SPI_FREQUENCY))
    {
        Serial.println("! SDW init failed");
        led.G();
        return;
    }
}

static int __lcd_r(const char *t, uint8_t size)
{
    return 128 - (int)strlen(t) * 6 * size;
}

void render()
{
    if (!lcd.ok())
        return;
    char buf[24];
    lcd.clear();

    snprintf(buf, sizeof(buf), "%.1f", s.flow);
    lcd.print(buf, 0, 0, 3);
    snprintf(buf, sizeof(buf), "%02u:%02u", s.h, s.m);
    lcd.print(buf, __lcd_r(buf, 1), 0);
    snprintf(buf, sizeof(buf), "%.1fC", s.t);
    lcd.print(buf, __lcd_r(buf, 1), 8);
    lcd.print("l/m", 110, 16);
    snprintf(buf, sizeof(buf), "#%lu", (unsigned long)s.pulseCount);
    lcd.print(buf, 0, 24);
    snprintf(buf, sizeof(buf), "%.1f/%.0fGB", s.sd_used, s.sd_size);
    lcd.print(buf, __lcd_r(buf, 1), 24);
}

void loop()
{
    static uint32_t display_ms = 0;
    if (millis() - display_ms >= SSD1306_FREQUENCY)
    {
        lcd.clear();
        display_ms = millis();
        render();
        lcd.refresh();
    }
    s.t = bme.readTemperature();
    s.sd_size = sdw.size();
    s.sd_used = sdw.used();
}