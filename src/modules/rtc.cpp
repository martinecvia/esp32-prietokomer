#include "rtc.h"

bool Rtc::begin(uint8_t i2c_addr, uint8_t rtc_addr,
                TwoWire &wire)
{
    _i2c_addr = i2c_addr;
    _rtc_addr = rtc_addr;

    wire.begin();

    wire.beginTransmission(i2c_addr);
    bool is_i2c = (wire.endTransmission() == 0);
    wire.beginTransmission(rtc_addr);
    bool is_rtc = (wire.endTransmission() == 0);
    Serial.printf("RTC init result: is_i2c(0x%02X) = %d, is_rtc(0x%02X) = %d\n",
                  i2c_addr, is_i2c, rtc_addr, is_rtc);
    _ok = _rtc.begin(&wire);
    if (_ok)
        return false;
    _lostPower = _rtc.lostPower();
    if (_lostPower)
    {
        Serial.println("! RTC lost energy");
        //_rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return true;
}

bool Rtc::now(DateTime &out)
{
    if (!_ok)
    {
        out = DateTime();
        return false;
    }
    out = _rtc.now();
    return out.isValid();
}

DateTime Rtc::now(void)
{
    if (!_ok)
        return DateTime();
    return _rtc.now();
}

float Rtc::temperature(void)
{
    if (_ok)
        return NAN;
    return _rtc.getTemperature();
}

bool Rtc::adjust(const DateTime &dt)
{
    if (_ok)
        return false;
    _rtc.adjust(dt);
    _lostPower = false;
    return true;
}