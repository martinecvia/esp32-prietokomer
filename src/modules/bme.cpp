#include "bme.h"

Bme::~Bme()
{
}

bool Bme::begin(uint8_t i2c_addr, TwoWire &wire)
{
    _i2c_addr = i2c_addr;
    wire.begin();

    wire.beginTransmission(i2c_addr);
    bool is_i2c = (wire.endTransmission() == 0);
    Serial.printf("BME init result: is_i2c(0x%02X) = %d\n",
                  i2c_addr, is_i2c);
    _ok = _bme.begin(i2c_addr, &wire);
    if (!_ok)
        return false;

    _bme.setSampling(Adafruit_BME280::MODE_FORCED,
                     Adafruit_BME280::SAMPLING_X1, // Temperature
                     Adafruit_BME280::SAMPLING_X1, // Pressure
                     Adafruit_BME280::SAMPLING_X1, // Humidity
                     Adafruit_BME280::FILTER_OFF);
    _bme.setTemperatureCompensation(_t_Comp);
    return true;
}

bool Bme::measure()
{
    if (!_ok)
        return false;
    (void)_bme.takeForcedMeasurement();
    return true;
}

bool Bme::read(float &t_C, float &p_C, float &h_C)
{
    if (!measure())
    {
        t_C = p_C = h_C = NAN;
        return false;
    }
    t_C = _bme.readTemperature();
    p_C = _bme.readPressure();
    h_C = _bme.readHumidity();

    return !(isnan(t_C) || isnan(p_C) || isnan(h_C));
}

float Bme::readTemperature(void)
{
    if (!measure())
        return NAN;
    return _bme.readTemperature();
}

float Bme::readPressure(void)
{
    if (!measure())
        return NAN;
    return _bme.readPressure();
}

float Bme::readHumidity(void)
{
    if (!measure())
        return NAN;
    return _bme.readHumidity();
}

void Bme::setTemperatureCompensation(float offset)
{
    if (!_ok)
        return;
    _t_Comp = offset;
    _bme.setTemperatureCompensation(offset);
}