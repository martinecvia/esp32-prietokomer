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
    Bme() = default;
    ~Bme() = default;

    Bme(const Bme &) = delete;
    Bme &operator=(const Bme &) = delete;

    bool begin(uint8_t i2c_addr = 0x76, TwoWire &wire = Wire);
    bool ok(void) const { return _ok; }

    bool measure(void);
    bool read(float &t_C, float &p_C, float &h_C);

    float readTemperature(void); // °C
    float readPressure(void);    // Pa
    float readHumidity(void);    // %RH

    float getTemperatureCompensation(void) const { return _t_Comp; }
    void setTemperatureCompensation(float offset);

private:
    uint8_t _i2c_addr = 0x76;
    Adafruit_BME280 _bme;
    bool _ok = false;
    float _t_Comp = 0.0f;
};