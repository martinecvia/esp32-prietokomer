#include <Wire.h>
#include "rtc.h"

Rtc::~Rtc()
{
}

bool Rtc::begin(uint8_t i2c_addr, uint8_t rtc_addr)
{
    _i2c_addr = i2c_addr;
    _rtc_addr = rtc_addr;
    Wire.beginTransmission(i2c_addr);
    bool is_i2c = (Wire.endTransmission() == 0);
    Wire.beginTransmission(rtc_addr);
    bool is_rtc = (Wire.endTransmission() == 0);
    Serial.printf("RTC init result: is_i2c(0x%02X) = %d, is_rtc(0x%02X) = %d\n",
                  i2c_addr, is_i2c, rtc_addr, is_rtc);
    _ok = is_i2c && is_rtc && _rtc.begin(&Wire);
    if (_ok && _rtc.lostPower())
    {
        Serial.println("! RTC lost energy");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return _ok;
}

DateTime Rtc::now(void)
{
    if (!_ok)
        return DateTime(F(__DATE__), F(__TIME__));
    return _rtc.now();
}