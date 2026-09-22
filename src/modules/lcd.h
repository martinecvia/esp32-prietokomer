#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_SH110X.h>

template <typename T>
struct LcdInitializer
{
    static bool begin(T &lcd, uint8_t addr);
};

/*
https://dratek.cz/arduino-platforma/1479-iic-i2c-displej-oled-0.91-128x32-bily-3-3-v-5v-pro-iot-arduino-raspbery.html
https://navody.dratek.cz/navody-k-produktum/oled-i2c-displej-128x32.html
https://www.youtube.com/watch?v=L5b-xK5axEI
Perfektní OLED displej na sběrnici I2C, displej nepotřebuje podsvícení, světélkují jednotlivé body, zaujme hned na první pohled oproti klasickým LCD a má velice nízkou spotřebu.

Specifikace produktu:

Driver IC: SSD1306
Velikost: 0,91 OLED
Rozměr: 128 x 32
Rozhraní: IIC
Barva displeje: bílá
Definice pinů:

GND: Power Ground
VCC: Power + (DC 3.3 ~5v)
SCL: Clock Line
SDA: Data Line
*/
template <>
struct LcdInitializer<Adafruit_SSD1306>
{
    static bool begin(Adafruit_SSD1306 &lcd, uint8_t addr)
    {
        return lcd.begin(SSD1306_SWITCHCAPVCC, addr);
    }
};

/*
https://dratek.cz/arduino-platforma/3181-iic-i2c-oled-1-3-displej-128x64-bily.html
OLED displej s bílými znaky má v porovnání s alfanumerickými LCD displeji mnoho výhod.

Patří mezi ně například vyšší rozlišení (v tomto případě 128x64 bodů) či nižší spotřeba, právě nižší spotřeby je dosaženo tím, že u OLED displeje svítí pouze ty body, které jsou aktivovány.

Specifikace:
Úhlopříčka: 1,3"
Rozlišení: 128 x 64
Čip: SSH1106
Rozměry displeje: 29,42 x 14,7 mm
Napětí: 3,3 - 5V
Pracovní teplota: -20 až 70°C
Rozhraní: IIC
Barva: bílá
Piny:

GND
VCC
SCK
SDA
*/
template <>
struct LcdInitializer<Adafruit_SH1106G>
{
    static bool begin(Adafruit_SH1106G &lcd, uint8_t addr)
    {
        return lcd.begin(addr, true);
    }
};

template <typename T>
class Lcd
{
public:
    Lcd(uint16_t width, uint16_t height) : _lcd(width, height, &Wire, -1) {}
    ~Lcd() = default;

    bool begin(uint8_t i2c_addr = 0x3C)
    {
        _i2c_addr = i2c_addr;
        Wire.beginTransmission(i2c_addr);
        bool is_i2c = (Wire.endTransmission() == 0);
        Serial.printf("LCD init result: is_i2c(0x%02X) = %d\n",
                      i2c_addr, is_i2c);
        _ok = LcdInitializer<T>::begin(_lcd, i2c_addr);

        if (_ok)
        {
            _lcd.clearDisplay();
            _lcd.cp437(true);
            _lcd.setTextColor(1);
            _lcd.setTextSize(1);
            _lcd.display();
        }
        return _ok;
    }
    bool ok() const { return _ok; }

    void clear(void)
    {
        if (!_ok)
            return;

        _lcd.clearDisplay();
    }

    void print(const String &text, int x, int y,
               uint8_t size = 1, uint16_t color = 1)
    {
        if (!_ok)
            return;

        _lcd.setTextColor(color);
        _lcd.setTextSize(size);
        _lcd.setCursor(x, y);
        _lcd.print(text);
    }

    void refresh(void)
    {
        if (!_ok)
            return;

        _lcd.display();
    }

private:
    uint8_t _i2c_addr;
    T _lcd;
    bool _ok = false;
};