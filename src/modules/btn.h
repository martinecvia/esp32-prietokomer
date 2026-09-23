class Btn
{
public:
    bool begin(uint8_t pin, uint32_t debounceMs = 30)
    {
        _pin = pin;
        _debounceMs = debounceMs;
        pinMode(pin, INPUT);
        _ok = true;
        return _ok;
    }

    bool ok(void) const { return _ok; };
    bool pressed(void)
    {
        if (!_ok)
            return false;
        bool pressed = (digitalRead(_pin) == LOW);

        uint32_t time = millis();
        if (pressed && !_checked)
            _delayed = true;
        else if (!pressed && _checked && _delayed)
        {
            _delayed = true;
            _lastDebounceMs = time;
            _waiting = true;
        }
        _checked = pressed;
        if (_waiting)
        {
            if (time - _lastDebounceMs >= _debounceMs)
            {
                _waiting = false;
                return true;
            }
        }
        return false;
    }

private:
    uint8_t _pin = 0;
    bool _ok = false;

    bool _delayed = false;
    bool _waiting = false;
    bool _checked = false;

    uint32_t _debounceMs = 30;
    uint32_t _lastDebounceMs = 0;
};