#include "isr.h"

void Isr::clear(void)
{
    portENTER_CRITICAL(&_mux);
    _used = 0;
    portEXIT_CRITICAL(&_mux);
}

bool Isr::begin(uint8_t pin, uint32_t debounceMs,
                uint8_t mode)
{
    pause();
    _pin = pin;
    _debounceUs = (int64_t)debounceMs * 1000;
    pinMode(_pin, mode);
    clear();

    _ok = true;
    return _ok;
}

// Volat POD _mux
void IRAM_ATTR Isr::add(void)
{
    _used++;
    _size++;
    if (_used > _peak)
        _peak = _used;
}

void IRAM_ATTR Isr::handler(void *arg)
{
    Isr *self = static_cast<Isr *>(arg);
    int64_t t = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&self->_mux);
    if (!self->_paused && t - self->_lastDebounceUs >= self->_debounceUs)
    {
        self->_lastDebounceUs = t;
        self->add();
    }
    portEXIT_CRITICAL_ISR(&self->_mux);
}

void Isr::pulse(void)
{
    if (!_ok)
        return;
    portENTER_CRITICAL(&_mux);
    if (!_paused)
        add();
    portEXIT_CRITICAL(&_mux);
}

bool Isr::pop(IsrEvent &event, const DateTime &dt)
{
    if (!_ok)
        return false;
    bool ok = false;
    portENTER_CRITICAL(&_mux);
    if (_used > 0)
    {
        _used--;
        ok = true;
    }
    portEXIT_CRITICAL(&_mux);
    if (ok)
        event.dt = dt;
    return ok;
}

uint32_t Isr::pending(void)
{
    portENTER_CRITICAL(&_mux);
    uint32_t used = _used;
    portEXIT_CRITICAL(&_mux);
    return used;
}

IsrStats Isr::stats(void)
{
    IsrStats s;
    portENTER_CRITICAL(&_mux);
    s.size = _size;
    s.used = _used;
    s.peak = _peak;
    portEXIT_CRITICAL(&_mux);
    return s;
}

void Isr::allow(void)
{
    if (!_ok || !_paused)
        return;
    portENTER_CRITICAL(&_mux);
    _lastDebounceUs = esp_timer_get_time();
    portEXIT_CRITICAL(&_mux);
    _paused = false;
    attachInterruptArg(digitalPinToInterrupt(_pin), handler, this, FALLING);
}

void Isr::pause(void)
{
    if (!_ok || _paused)
        return;
    detachInterrupt(digitalPinToInterrupt(_pin));
    _paused = true;
}