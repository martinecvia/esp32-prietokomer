#include <Wire.h>
#include <Arduino.h>

#include "btn.h"
#include "cfg.h"
#include "isr.h"
#include "flow_meter.h"
#include "flow_meter_test.h"
using namespace flow_meter_test;
#include "web.h"

#include "modules/led.h"
#include "modules/lcd.h"
#include "modules/rtc.h"
#include "modules/bme.h"
#include "modules/sdw.h"
#include "modules/sdw_test.h"
using namespace sdw_test;

Led led;
Lcd<Adafruit_SH1106G> lcd(128, 64);
Rtc rtc;
Bme bme;
Sdw sdw;
Btn bt1, bt2;

Isr isr;
FlowMeter flow(1.0f); // K = 1
FlowMeterTest fmt([]
                  { isr.pulse(); });
uint32_t pulseDelta = UINT32_MAX;

Web web;

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
    if (s == state)
        return;
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

void makeNewFile(const DateTime &dt)
{
    char filename[32];
    snprintf(filename, sizeof(filename), "/flowrate_%04u-%02u-%02u_%02u-%02u.csv",
             dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
    if (!sdw.openNewFile(filename))
        return;
    sdw.writeLine("Date_time_Impulse;Impulse;T_Diferences_T_sec;Total_Volume_l;Flow_rate;Average_FR_per_min;Temperature;Rtc_Flag");
    Serial.printf("SDW file: path = %s\n", sdw.filepath().c_str());
}

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
    case SdwEvent::CardMounted:
        if (sdw.filename().length() == 0)
            makeNewFile(rtc.now());
        setState(SystemState::RUNNING);
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

    web.begin(sdw, WIFI_AP_SSID, WIFI_AP_PASSWORD);
    isr.begin(PIN_CYBLE_NF1, CYBLE_NF1_DEBOUNCE_MS, INPUT_PULLUP);
    bt1.begin(PIN_BUTTON_1, BUTTON_DEBOUNCE_MS);
    bt2.begin(PIN_BUTTON_2_CYBLE_NF1, BUTTON_DEBOUNCE_MS);

    // SDW
    sdw.onEvent(onSdwEvent);
    if (!sdw.begin(PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, true, SPI_FREQUENCY) ||
        !sdwSelfTest(sdw, Serial, 16))
    {
        Serial.println("! SDW init failed");
        setState(SystemState::ERROR);
        return;
    }
    makeNewFile(now);
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

    if (web.is_serving())
    {
        snprintf(buf, sizeof(buf), "ssid: %s", web.ssid());
        lcd.print(buf, 0, 0);
        snprintf(buf, sizeof(buf), "pass: %s", web.pass());
        lcd.print(buf, 0, 8);
        snprintf(buf, sizeof(buf), "http://%s/", web.ip().toString().c_str());
        lcd.print(buf, 0, 16);
        return;
    }
    snprintf(buf, sizeof(buf), "%.1f", s.lpm);
    lcd.print(buf, 0, 0, 2);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", s.h, s.m, s.s);
    lcd.print(buf, __lcd_r(buf, 1), 0);
    snprintf(buf, sizeof(buf), "%02u.%02u", s.D, s.M);
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
        while (isr.pop(e, rtc.now()))
        {
            uint32_t t0 = flow.since(e.dt);
            flow.add(e.dt);
            s.pulseCount++;

            char line[64];
            // "Date_time_Impulse;Impulse;T_Diferences_T_sec;Total_Volume_l;Flow_rate;Flow_rate_exp;Temperature;Rtc_Flag"
            snprintf(line, sizeof(line),
                     "%02u.%02u.%04u %02u:%02u:%02u;%lu;%lu;%lu;%lu;%.1f;%.1f;%lu",
                     e.dt.day(), e.dt.month(), e.dt.year(), e.dt.hour(), e.dt.minute(), e.dt.second(),
                     (int)1, (unsigned long)t0,
                     (unsigned long)s.pulseCount, (unsigned long)flow.get(e.dt), flow.lpm(e.dt),
                     rtc.temperature(), (int)rtc.lostPower());
            sdw.writeLine(line);

            led.Y();
            __led_is_working = true;
            led_update = millis();
            Serial.printf("ISR call: %s\n", line);
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

    // WEB
    web.loop();
    if (state == SystemState::PAUSED && !web.is_serving())
        if (web.start())
            Serial.printf("WEB conn: %s >> http://%s/\n", web.ssid(), web.ip().toString().c_str());
        else
            Serial.println("! WEB conn failed");
    else if (state != SystemState::PAUSED && web.is_serving())
        web.pause();

    // BTN
    if (bt1.pressed())
        if (state == SystemState::RUNNING)
            setState(SystemState::PAUSED);
        else if (state == SystemState::PAUSED)
            setState(SystemState::RUNNING);
    if (bt2.pressed())
        isr.pulse();
}