#include <Wire.h>
#include <Arduino.h>

#include "btn.h"
#include "cfg.h"
#include "isr.h"
#include "flow_meter.h"
#include "flow_meter_test.h"
using namespace flow_meter_test;

#include "modules/led.h"
#include "modules/lcd.h"
#include "modules/rtc.h"
#include "modules/bme.h"
#include "modules/sdw.h"
#include "modules/sdw_test.h"
using namespace sdw_test;

Led led;
Lcd<Adafruit_SSD1306> lcd(128, 32);
Rtc rtc;
Bme bme;
Sdw sdw;
Btn bt1, bt2;

Isr isr;
FlowMeter flow(1.0f); // K = 1
FlowMeterTest fmt([]
                  { isr.pulse(); });

enum class SystemState
{
    NONE,
    RUNNING,
    PAUSED,
    ERROR
};

struct __lcd_display_state
{
    // Environment
    float t = 0.0f;

    // Clock
    uint8_t h = 0, m = 0, s = 0;
    uint8_t D = 0, M = 0;

    // FlowMeter
    uint32_t get = 0;
    float lpm = 0.0f;
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
    case SystemState::NONE:
        led.Y();
        break;
    case SystemState::RUNNING:
        led.G();
        break;
    case SystemState::PAUSED:
        led.set(0, 1, 1);
        break;
    case SystemState::ERROR:
        led.R();
        break;
    }
}

void setState(SystemState s)
{
    state = s;
    if (state == SystemState::RUNNING)
        isr.allow();
    else
        isr.pause();
    changeLedForState();
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
        setState(SystemState::ERROR);
        break;
    default:
        break;
    }
}

void setup()
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
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
    // RTC NTP Magic
    if (rtc.lostPower())
        rtc.adjust(WIFI_AP_SSID, WIFI_AP_PASSWORD, 5000);
    DateTime now = rtc.now();
    Serial.printf("RTC time: %02u:%02u:%02u %02u/%02u/%02u\n",
                  now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());

    sdw.onEvent(onSdwEvent);
    if (!sdw.begin(PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, true, SPI_FREQUENCY) ||
        !sdwSelfTest(sdw, Serial, 16))
    {
        Serial.println("! SDW init failed");
        setState(SystemState::ERROR);
        return;
    }
    Serial.printf("SDW file: name = %s, path = \n", sdw.filename().c_str(), sdw.filepath().c_str());

    // ISR
    if (!isr.begin(PIN_CYBLE_NF1, CYBLE_NF1_DEBOUNCE_MS, INPUT_PULLUP))
    {
        Serial.println("! ISR init failed");
        setState(SystemState::ERROR);
        return;
    }

    // fmt.test(12.0f);
    bt1.begin(PIN_BUTTON_1, BUTTON_DEBOUNCE_MS);
    bt2.begin(PIN_BUTTON_2_CYBLE_NF1, BUTTON_DEBOUNCE_MS);
    setState(SystemState::RUNNING);
}

int __lcd_r(const char *t, uint8_t size)
{
    return lcd.w() - (int)strlen(t) * 6 * size;
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

    s.get = flow.get(now);
    s.lpm = flow.lpm(now);
}

void render(void)
{
    if (!lcd.ok())
        return;
    char buf[24];
    lcd.clear();

    snprintf(buf, sizeof(buf), "%lu|%.1f", s.get, s.lpm);
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

    if (isr.pending())
    {
        IsrEvent e;
        DateTime dt = rtc.now();
        while (isr.pop(e, dt))
        {
            flow.add(e.dt);
            s.pulseCount++;
            led.Y();
            __led_is_working = true;
            led_update = millis();
            Serial.printf("ISR call: #%lu %02u:%02u:%02u - flow = %lu|%.1f l/m, unix = %lu\n",
                          (unsigned long)s.pulseCount,
                          e.dt.hour(), e.dt.minute(), e.dt.second(),
                          flow.get(dt), flow.lpm(dt),
                          (unsigned long)dt.unixtime());
        }
    }

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
        if (state == SystemState::RUNNING)
            setState(SystemState::PAUSED);
        else if (state == SystemState::PAUSED)
            setState(SystemState::RUNNING);
    if (bt2.pressed())
        isr.pulse();
}