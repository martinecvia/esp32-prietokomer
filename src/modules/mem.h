#pragma once
#include <Arduino.h>

#include <Wire.h>

class Mem
{
public:
    Mem() = default;
    Mem(const Mem &) = delete;
    Mem &operator=(const Mem &) = delete;

    bool begin(uint8_t i2c_addr = 0x50, TwoWire &wire = Wire);
    bool ok() const { return _ok; }
    static constexpr size_t length() { return MEMORY_SIZE; }
    bool erase(void);

    template <typename T>
    bool get(size_t addr, T &t) { return fetch(addr, (uint8_t *)&t, sizeof(T)); }
    template <typename T>
    bool put(size_t addr, const T &t) { return write(addr, (const uint8_t *)&t, sizeof(T)); }

    Mem &withReadWriteTries(size_t n)
    {
        readWriteTries = n;
        return *this;
    }
    Mem &withCompletionTries(size_t n)
    {
        completionTries = n;
        return *this;
    }

    bool fetch(size_t addr, uint8_t *data, size_t length);
    bool write(size_t addr, const uint8_t *data, size_t length);

private:
    static constexpr size_t BLOCK_BITS = 7;
    static constexpr size_t BLOCK_SIZE = 1u << BLOCK_BITS;
    static constexpr size_t NUM_BLOCKS = 1;
    static constexpr size_t MEMORY_SIZE = BLOCK_SIZE * NUM_BLOCKS;
    static constexpr size_t PAGE_SIZE = 8;
    static constexpr size_t READ_CHUNK = 32;

    uint8_t devAddr(size_t addr) const { return (uint8_t)(_i2c_addr | (addr >> BLOCK_BITS)); }
    bool setAddress(size_t addr);

    TwoWire *_wire = &Wire;
    uint8_t _i2c_addr = 0x50;
    bool _ok = false;
    size_t readWriteTries = 5;
    size_t completionTries = 100;
};