#include <Wire.h>
#include <Arduino.h>

#include "cfg.h"
#include "modules/led.h"
#include "modules/lcd.h"
#include "modules/rtc.h"
#include "modules/bme.h"
#include "modules/sdw.h"
#include "modules/btn.h"

Led led;
Lcd<Adafruit_SSD1306> lcd(128, 32);
Rtc rtc;
Bme bme;
Sdw sdw;
Btn bt1, bt2, bt3;

enum class SystemState
{
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
    float sd_used = 0.0f;
    float sd_size = 0.0f;
};

SystemState state = SystemState::IDLE;
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
    case SystemState::IDLE:
    case SystemState::PAUSED:
        led.Y();
        break;
    }
}

__lcd_display_state s;
bool __led_is_working = false;
uint32_t __led_millis = 0;

void onSdwEvent(SdwEvent event)
{
    switch (event)
    {
    case SdwEvent::WriteOk:
        led.off();
        __led_is_working = true;
        __led_millis = millis();
        break;
    case SdwEvent::WriteError:
    case SdwEvent::CardRemoved:
    case SdwEvent::Error:
        if (state == SystemState::ERROR)
            return;
        // capture - turn off
        sdw.close_file();
        state = SystemState::ERROR;
        // wifi - turn off
        __led_is_working = false;
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

    // Required
    if (!rtc.begin(__ADDR_I2C_AT24C32, __ADDR_I2C_DS3231))
    {
        Serial.println("! RTC init failed");
        state = SystemState::ERROR;
        changeLedForState();
        return;
    }
    sdw.call(onSdwEvent);
    if (!sdw.begin(PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, true, SPI_FREQUENCY))
    {
        Serial.println("! SDW init failed");
        state = SystemState::ERROR;
        changeLedForState();
        return;
    }
    bt1.begin(PIN_BUTTON_1, BUTTON_DEBOUNCE_MS);
    bt2.begin(PIN_BUTTON_2, BUTTON_DEBOUNCE_MS);
    bt3.begin(PIN_BUTTON_3_CYBLE_NF1, BUTTON_DEBOUNCE_MS);
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
    s.t = bme.readTemperature();
    s.sd_size = sdw.size();
    s.sd_used = sdw.used();
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
    snprintf(buf, sizeof(buf), "%s/%s", __format_b(s.sd_used), __format_b(s.sd_size));
    lcd.print(buf, __lcd_r(buf, 1), 24);
}

void loop()
{
    update();
    if (bt1.pressed())
        Serial.println("HWD nbutton 1");
    if (bt2.pressed())
        Serial.println("HWD nbutton 2");
    if (bt3.pressed() && state == SystemState::RUNNING)
    {
        Serial.println("HWD nbutton 3 (TEST)");
        led.Y();
        __led_is_working = true;
        __led_millis = millis();
        s.pulseCount++;
    }
    if (__led_is_working && millis() - __led_millis >= 100)
    {
        __led_is_working = false;
        changeLedForState();
    }
    static uint32_t display_ms = 0;
    if (millis() - display_ms >= SSD1306_FREQUENCY)
    {
        lcd.clear();
        display_ms = millis();
        render();
        lcd.refresh();
    }
}