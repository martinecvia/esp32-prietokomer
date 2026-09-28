#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <esp_timer.h>

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

struct IsrEvent
{
    DateTime dt;
};

struct IsrStats
{
    uint32_t size; // Celkový počet pulzů
    uint32_t used; // V queue
    uint32_t peak; // Největší počet pulzů v queue
};

class Isr
{
public:
    bool begin(uint8_t pin, uint32_t debounceMs = 15,
               uint8_t mode = INPUT_PULLUP);
    bool ok() const { return _ok; }

    void allow(void);
    bool is_allow(void) const { return !_paused; }
    void pause(void);
    void clear(void);

    void pulse(void);
    bool pop(IsrEvent &event, const DateTime &dt);

    uint32_t pending(void);
    IsrStats stats(void);

private:
    uint8_t _pin;
    int64_t _debounceUs = 15000, _lastDebounceUs = 0;
    bool _ok = false;

    static void IRAM_ATTR handler(void *arg);
    void IRAM_ATTR add(void); // Volat POD _mux

    volatile bool _paused = true;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    uint32_t _size = 0; // total
    uint32_t _used = 0; // pending
    uint32_t _peak = 0; // peak
};