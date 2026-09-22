#pragma once

#include <Arduino.h>
#include <Adafruit_BME280.h>

/*
Specifikace:
Napájecí napětí: 1,8 - 5V DC
Rozhraní: I2C (až 3.4MHz)
Provozní rozsah:
Teplota: -40 až +85°C
Vlhkost: 0 až 100%
Tlak: 30 - 110 kPa (0,3 – 1,1 bar)
Rozlišení:
Teplota: 0.01°C
Vlhkost: 0,008%
Tlak: 0.18Pa
Přesnost:
Teplota: + 1°C
Vlhkost: + 3%
Tlak: + -1Pa
I2C adresa:
SDO LOW: 0x76
SDO HIGH: 0x77
*/
class Bme
{
public:
    ~Bme();
    bool begin(uint8_t i2c_addr = 0x76);
    bool ok(void) const { return _ok; }

    float readTemperature(void);
    float readTemperatureCompensation(void);
    float readPressure(void);
    float readHumidity(void);

    void temperatureCompensation(float k);

private:
    uint8_t _i2c_addr;
    Adafruit_BME280 _bme;
    bool _ok = false;
};