#include "bme.h"

Bme::~Bme()
{
}

bool Bme::begin(uint8_t i2c_addr)
{
    _i2c_addr = i2c_addr;
    Wire.beginTransmission(i2c_addr);
    bool is_i2c = (Wire.endTransmission() == 0);
    Serial.printf("BME init result: is_i2c(0x%02X) = %d\n",
                  i2c_addr, is_i2c);
    if (!_bme.begin(i2c_addr))
    {
        _ok = false;
        return false;
    }
    _ok = is_i2c;
    return _ok;
}

float Bme::readTemperature(void)
{
    if (!_ok)
        return 0.0f;
    return _bme.readTemperature();
}

float Bme::readPressure(void)
{
    if (!_ok)
        return 0.0f;
    return _bme.readPressure();
}

float Bme::readHumidity(void)
{
    if (!_ok)
        return 0.0f;
    return _bme.readHumidity();
}

float Bme::readTemperatureCompensation(void)
{
    if (!_ok)
        return 0.0f;
    return _bme.getTemperatureCompensation();
}

void Bme::temperatureCompensation(float k)
{
    if (!_ok)
        return;
    _bme.setTemperatureCompensation(k);
}