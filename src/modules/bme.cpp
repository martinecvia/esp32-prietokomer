#include "bme.h"

Bme::~Bme()
{
}

bool Bme::begin(uint8_t i2c_addr)
{
    if (!_bme.begin(i2c_addr))
    {
        _ok = false;
        return false;
    }
    _ok = true;
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