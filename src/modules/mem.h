#pragma once

#include <Arduino.h>

class Mem
{
public:
    Mem() = default;
    ~Mem() = default;

    Mem(const Mem &) = delete;
    Mem &operator=(const Mem &) = delete;
    bool begin(const uint8_t i2c_addr = 0x50, TwoWire &wire = Wire);
    bool ok(void) const { return _ok; }

private:
    uint8_t _i2c_addr;
    bool _ok = false;
};