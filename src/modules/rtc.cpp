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
    _ok = is_i2c && is_rtc;
    return _ok;
}

byte Rtc::decBcd(byte val)
{
    return ((val / 10 * 16) + (val % 10));
}

byte Rtc::bcdDec(byte val)
{
    return ((val / 16 * 10) + (val % 16));
}

void Rtc::setRtcTime(byte s, byte m, byte h,
                     byte D, byte d, byte M, byte Y)
{
}

void Rtc::getRtcTime(byte *s, byte *m, byte *h,
                     byte *d, byte *D, byte *M, byte *Y)
{
    Wire.beginTransmission(_i2c_addr);
    Wire.write(0);
    Wire.endTransmission();
    Wire.requestFrom(_i2c_addr, 7);
    *s = bcdDec(Wire.read() & 0x7f);
    *m = bcdDec(Wire.read());
    *h = bcdDec(Wire.read() & 0x3f);
    *d = bcdDec(Wire.read());
    *D = bcdDec(Wire.read());
    *M = bcdDec(Wire.read());
    *Y = bcdDec(Wire.read());
}

void Rtc::displayTime()
{
    byte s, m, h, d, D, M, Y;
    getRtcTime(&s, &m, &h, &d, &D, &M, &Y);
    if (h < 10)
    {
        Serial.print("0");
    }
    Serial.print(h, DEC);
    Serial.print(":");
    if (m < 10)
    {
        Serial.print("0");
    }
    Serial.print(m, DEC);
    Serial.print(":");
    if (s < 10)
    {
        Serial.print("0");
    }
    Serial.print(s, DEC);
    Serial.print(" ");
    Serial.print(D, DEC);
    Serial.print("/");
    Serial.print(M, DEC);
    Serial.print("/");
    Serial.print(Y, DEC);
}