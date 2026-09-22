#include "led.h"

bool Led::begin(uint8_t pin_r, uint8_t pin_y, uint8_t pin_g)
{
    _pin_r = pin_r;
    _pin_y = pin_y;
    _pin_g = pin_g;

    pinMode(_pin_r, OUTPUT);
    pinMode(_pin_y, OUTPUT);
    pinMode(_pin_g, OUTPUT);
    _ok = true;

    off();
    return _ok;
}

void Led::setPin(uint8_t pin, bool state)
{
    digitalWrite(pin, state ? HIGH : LOW);
}

void Led::set(bool r, bool y, bool g)
{
    if (!_ok)
        return;
    setPin(_pin_r, r);
    setPin(_pin_y, y);
    setPin(_pin_g, g);
}

void Led::off(void)
{
    if (!_ok)
        return;
    setPin(_pin_r, 0);
    setPin(_pin_y, 0);
    setPin(_pin_g, 0);
}

Led::~Led()
{
    off();
}

void Led::R() { set(1, 0, 0); };
void Led::Y() { set(0, 1, 0); };
void Led::G() { set(0, 0, 1); };