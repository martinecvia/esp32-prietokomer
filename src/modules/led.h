#pragma once

#include <Arduino.h>

/*
https://dratek.cz/arduino-platforma/7719-modul-led-semafor.html
LED semafor je mimořádně jednoduchý na zapojení, 3 piny pro napájení LED a jeden pin GND pro uzemnění.

LED jsou připojeny přes ochranný rezistor, takže je možné modul připojit přímo k pinům Arduina.

Specifikace:
Průměr LED: 8 mm
*/
class Led
{
public:
    ~Led();
    bool begin(uint8_t pin_r, uint8_t pin_y, uint8_t pin_g);
    bool ok(void) const { return _ok; }

    void set(bool r, bool y, bool g);
    void off(void);

    void R(void);
    void Y(void);
    void G(void);

private:
    bool _ok = false;
    uint8_t _pin_r;
    uint8_t _pin_y;
    uint8_t _pin_g;

    void setPin(uint8_t pin, bool state);
};