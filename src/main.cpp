#include <Wire.h>
#include <Arduino.h>

#include "cfg.h"
#include "modules/led.h"
#include "modules/lcd.h"
#include "modules/rtc.h"
#include "modules/bme.h"
#include "modules/sdw.h"
#include "modules/sdw_test.h"
using namespace sdw_test;
#include "modules/btn.h"

Led led;
Lcd<Adafruit_SSD1306> lcd(128, 32);
Rtc rtc;
Bme bme;
Sdw sdw;
Btn bt1, bt2;

enum class SystemState
{
    NONE,
    IDLE,
    RUNNING,
    PAUSED,
    ERROR
};

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
    uint64_t sd_used = 0;
    uint64_t sd_size = 0;
};

SystemState state = SystemState::NONE;
void changeLedForState(void)
{
    switch (state)
    {
    case SystemState::ERROR:
        led.R();
        break;
    case SystemState::RUNNING:
        led.G();
        break;
    case SystemState::NONE:
        led.Y();
        break;
    case SystemState::IDLE:
    case SystemState::PAUSED:
        led.set(0, 1, 1);
        break;
    }
}

__lcd_display_state s;
bool __led_is_working = false;
uint32_t led_update = 0, lcd_update = 0, sdw_update = 0;

void onSdwEvent(SdwEvent event)
{
    if (state == SystemState::NONE)
        return;
    Serial.printf("SDW notify calls: %s\n", sdwEventName(event));
    switch (event)
    {
    case SdwEvent::WriteOk:
        break;
    case SdwEvent::FileOpened:
        break;
    case SdwEvent::WriteError:
    case SdwEvent::CardRemoved:
    case SdwEvent::Error:
        if (state == SystemState::ERROR)
            return;
        state = SystemState::ERROR;
        changeLedForState();
        break;
    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200); // Serial
    delay(300);           // 0.3s delay
    // LED
    Serial.println("\n\n\nsd_pulse_logger");
    if (!led.begin(PIN_LED_R, PIN_LED_Y, PIN_LED_G))
    {
        Serial.println("! LED init failed");
    }
    changeLedForState();

    // I2C
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQUENCY);
    for (byte addr = 1; addr < 127; addr++) // 7bit
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.print("I2C device found: 0x");
            if (addr < 16)
                Serial.print("0");
            Serial.println(addr, HEX);
        }
    }

    if (!lcd.begin(__ADDR_I2C_SSD1306))
    {
        Serial.println("! LCD init failed");
    }
    lcd.clear();

    if (!bme.begin(__ADDR_I2C_BME280))
    {
        Serial.println("! BME init failed");
    }

    if (!rtc.begin(__ADDR_I2C_AT24C32, __ADDR_I2C_DS3231))
    {
        Serial.println("! RTC init failed");
    }
    Serial.printf("RTC temp: %.1f\n", rtc.temperature());
    DateTime now = rtc.now();
    Serial.printf("RTC time: %02u:%02u:%02u %02u/%02u/%02u\n",
                  now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());

    sdw.onEvent(onSdwEvent);
    if (!sdw.begin(PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, true, SPI_FREQUENCY) ||
        !sdwSelfTest(sdw, Serial, 16))
    {
        Serial.println("! SDW init failed");
        state = SystemState::ERROR;
        changeLedForState();
        return;
    }
    Serial.printf("SDW file: name = %s, path = \n", sdw.filename().c_str(), sdw.filepath().c_str());
    bt1.begin(PIN_BUTTON_1, BUTTON_DEBOUNCE_MS);
    bt2.begin(PIN_BUTTON_2_CYBLE_NF1, BUTTON_DEBOUNCE_MS);
    state = SystemState::RUNNING;
    changeLedForState();
}

static int __lcd_r(const char *t, uint8_t size)
{
    return 128 - (int)strlen(t) * 6 * size;
}

static String __format_b(uint64_t size)
{
    const char *units[] = {"B", "KB", "MB", "GB"};
    double size_d = (double)size;
    int unit = 0;
    while (size_d >= 1024.0 && unit < 3)
    {
        size_d /= 1024.0;
        unit++;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f%s", size_d, units[unit]);
    return String(buf);
}

void update(void)
{
    DateTime now = rtc.now();
    s.h = now.hour();
    s.m = now.minute();
    s.s = now.second();
    s.D = now.day();
    s.M = now.month();
    s.t = rtc.temperature();
}

void render(void)
{
    if (!lcd.ok())
        return;
    char buf[24];
    lcd.clear();

    snprintf(buf, sizeof(buf), "%.1f", s.flow);
    lcd.print(buf, 0, 0, 3);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", s.h, s.m, s.s);
    lcd.print(buf, __lcd_r(buf, 1), 0);
    snprintf(buf, sizeof(buf), "%02u/%02u", s.D, s.M);
    lcd.print(buf, __lcd_r(buf, 1), 8);
    snprintf(buf, sizeof(buf), "%.1fC", s.t);
    lcd.print(buf, __lcd_r(buf, 1), 16);
    snprintf(buf, sizeof(buf), "#%lu", (unsigned long)s.pulseCount);
    lcd.print(buf, 0, 24);
    snprintf(buf, sizeof(buf), "%s/%s", __format_b(s.sd_used).c_str(), __format_b(s.sd_size).c_str());
    lcd.print(buf, __lcd_r(buf, 1), 24);
}

void loop()
{
    if (state == SystemState::NONE)
        return;

    // LED update
    if (__led_is_working && millis() - led_update >= 100)
    {
        __led_is_working = false;
        changeLedForState();
    }

    // LCD update
    if (lcd.ok() &&
        millis() - lcd_update >= SSD1306_FREQUENCY)
    {
        lcd_update = millis();
        update();
        render();
        lcd.refresh();
    }

    // SDW update
    if (sdw_update == 0 || millis() - sdw_update >= 15000)
    {
        sdw_update = millis();
        s.sd_size = sdw.size();
        s.sd_used = sdw.used();
    }

    // BTN
    if (bt1.pressed())
    {
        Serial.println("HWD button 1");
        state = SystemState::PAUSED;
        changeLedForState();
    }

    if (bt2.pressed())
    {
        Serial.println("HWD button 2");
        led.Y();
        __led_is_working = true;
        led_update = millis();
        s.pulseCount++;
    }
}