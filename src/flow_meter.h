#pragma once

#include <Arduino.h>
#include <esp_timer.h>

class FlowMeter
{
public:
    static constexpr uint8_t flow_t = 60;
    static constexpr uint8_t hist_t = 2 * flow_t;

    explicit FlowMeter(float k = 1.0f, float tau = 2.0f) : _k(k), _tau(tau) {}

    void add(const DateTime &dt)
    {
        uint32_t t = dt.unixtime();
        advance(t);

        uint16_t &b = _b[t % hist_t];
        if (b < UINT16_MAX)
            b++;
        _size++;
    }

    float lpm(const DateTime &dt, float tau = -1.0f)
    {
        float curr = raw(dt);
        uint32_t t = _t;
        float T = (tau >= 0.0f) ? tau : _tau;

        if (T <= 0.0f || !_ak || curr == 0.0f || _curr == 0.0f || t < _t0)
            _curr = curr;
        else if (t > _t0)
            _curr += (1.0f - expf(-(float)(t - _t0) / T)) * (curr - _curr);

        _t0 = t;
        _ak = true;
        return _curr;
    }

    float raw(const DateTime &dt)
    {
        advance(dt.unixtime());
        uint32_t t = _t;

        // Sekund od posledního pulzu
        uint32_t t0 = 0;
        while (t0 < flow_t && _b[(t - t0) % hist_t] == 0)
            t0++;
        if (t0 >= flow_t)
            return 0.0f; // 60s nic => 0
        uint32_t sLast = t - t0;

        // reverse()
        uint32_t n = 0, cFirst = 0, sFirst = sLast;
        for (uint32_t age = 0; age < flow_t; age++)
        {
            uint32_t s = sLast - age;
            uint16_t c = _b[s % hist_t];
            if (c == 0)
                continue;
            if (n >= 2 && age > 10)
                break; // Mimo námi definované okno
            n += c;
            sFirst = s;
            cFirst = c;
        }
        // Jeden pulz je málo abychom něco z toho zjistili
        if (n < 2)
            return 0.0f;

        // avg()
        uint32_t span = sLast - sFirst;
        float interval = (span == 0)
                             ? 1.0f / (float)(n - 1)              // Všechno v jedné sekundě
                             : (float)span / (float)(n - cFirst); // Pulzy za prvním / t

        // Průtok kulminuje, takže hodnota klesá
        if (t0 >= 2 && (float)(t0 - 1) > interval)
            interval = (float)(t0 - 1);

        return (60.0f / interval) * _k;
    }

    uint32_t get(const DateTime &dt)
    {
        advance(dt.unixtime());
        uint32_t sum = 0;
        for (uint32_t age = 0; age < flow_t; age++)
            sum += _b[(_t - age) % hist_t];
        return sum;
    }

    void reset(void)
    {
        memset(_b, 0, sizeof(_b));
        _ok = false;
        _ak = false;
        _t = 0;
        _t0 = 0;
        _size = 0;
        _curr = 0.0f;
    }

    uint32_t pulses() const { return _size; }
    float liters() const { return _size * _k; }

private:
    float _k, _tau;
    bool _ok = false;
    bool _ak = false;

    float _curr = 0.0f;
    uint32_t _t0 = 0; // Čas posledního lpm()

    uint32_t _size = 0;
    uint32_t _t = 0;          // Aktuální čas
    uint16_t _b[hist_t] = {}; // Buffer

    void advance(uint32_t t)
    {
        // Stejná nebo o chvilku starší sekunda => neposouvat
        if (_ok && t <= _t && (_t - t) < hist_t)
            return;
        // Dlouhá pauza => vynulovat
        if (!_ok || t < _t || (t - _t) >= hist_t)
        {
            memset(_b, 0, sizeof(_b));
            _t = t;
            _ok = true;
            return;
        }
        while (_t < t)
        {
            _t++;
            _b[_t % hist_t] = 0;
        }
    }
};