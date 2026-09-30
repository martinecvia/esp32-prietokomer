#include "mem.h"

bool Mem::begin(uint8_t i2c_addr, TwoWire &wire)
{
    _i2c_addr = i2c_addr;
    _wire = &wire;
    _wire->begin();

    _wire->beginTransmission(_i2c_addr);
    _ok = (_wire->endTransmission() == 0);
    Serial.printf("MEM init: 0x%02X %s\n", _i2c_addr, _ok ? "OK" : "NOT FOUND");
    return _ok;
}

bool Mem::erase(void)
{
    uint8_t buf[PAGE_SIZE];
    memset(buf, 0x00, sizeof(buf));
    for (size_t addr = 0; addr < MEMORY_SIZE; addr += PAGE_SIZE)
        if (!write(addr, buf, PAGE_SIZE))
            return false;
    return true;
}

bool Mem::setAddress(size_t addr)
{
    for (size_t t = 0; t < readWriteTries; t++)
    {
        _wire->beginTransmission(devAddr(addr));
        _wire->write((uint8_t)(addr & (BLOCK_SIZE - 1)));
        if (_wire->endTransmission(false) == 0)
            return true;
        delay(10);
    }
    return false;
}

bool Mem::fetch(size_t addr, uint8_t *data, size_t length)
{
    if (addr + length > MEMORY_SIZE)
        return false;

    while (length > 0)
    {
        // don't cross a block boundary in one transaction (matters for NUM_BLOCKS > 1)
        size_t n = min(length, min(READ_CHUNK, BLOCK_SIZE - (addr & (BLOCK_SIZE - 1))));

        if (!setAddress(addr))
            return false;
        if (_wire->requestFrom(devAddr(addr), (uint8_t)n, (uint8_t)true) != n)
            return false;

        for (size_t i = 0; i < n; i++)
            *data++ = _wire->read();
        addr += n;
        length -= n;
    }
    return true;
}

bool Mem::write(size_t addr, const uint8_t *data, size_t length)
{
    if (addr + length > MEMORY_SIZE)
        return false;

    while (length > 0)
    {
        size_t n = min(length, PAGE_SIZE - (addr & (PAGE_SIZE - 1)));
        uint8_t dev = devAddr(addr);

        bool sent = false;
        for (size_t t = 0; t < readWriteTries && !sent; t++)
        {
            _wire->beginTransmission(dev);
            _wire->write((uint8_t)(addr & (BLOCK_SIZE - 1)));
            _wire->write(data, n);
            sent = (_wire->endTransmission(true) == 0);
            if (!sent)
                delay(10);
        }
        if (!sent)
            return false;

        // ACK polling: wait for the internal write cycle (~5 ms) to finish
        bool done = false;
        for (size_t t = 0; t < completionTries && !done; t++)
        {
            _wire->beginTransmission(dev);
            done = (_wire->endTransmission(true) == 0);
            if (!done)
                delayMicroseconds(100);
        }
        if (!done)
            return false;

        addr += n;
        data += n;
        length -= n;
    }
    return true;
}