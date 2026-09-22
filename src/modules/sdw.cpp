#include "sdw.h"

Sdw::~Sdw()
{
    close_file();
    if (_mutex)
    {
        vSemaphoreDelete(_mutex);
    }
}

bool Sdw::lock(uint32_t timeout)
{
    if (!_useMutex || !_mutex)
        return true;
    return xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeout)) == pdTRUE;
}

void Sdw::release(void)
{
    if (_useMutex && _mutex)
        xSemaphoreGive(_mutex);
}

void Sdw::notify(SdwEvent event)
{
    _last = event;
    if (_callback)
        _callback(event);
}

void Sdw::close_file(void)
{
    if (!lock())
        return;
    bool wasopen = _is_open;
    if (_is_open)
    {
        _file.flush();
        _file.close();
        _is_open = false;
    }
    release();

    if (wasopen)
        notify(SdwEvent::FileClosed);
}

void Sdw::flush_file(void)
{
    if (!lock())
        return;
    if (_is_open)
    {
        _file.flush();
    }
    release();
}

// Volat pod lockem
bool Sdw::ensureCardReady(void)
{
    if (_mounted)
        return true;
    SD.end();
    SPI.begin(_clPin, _soPin, _siPin, _csPin);
    _ok = _mounted = SD.begin(_csPin, SPI, _freqHz);
    return _mounted;
}

bool Sdw::begin(uint8_t csPin, uint8_t clPin, uint8_t soPin, uint8_t siPin,
                bool useMutex, uint32_t freqHz)
{
    _csPin = csPin;
    _clPin = clPin;
    _soPin = soPin; // MISO
    _siPin = siPin; // MOSI

    _useMutex = useMutex;
    _freqHz = freqHz;

    if (_useMutex && !_mutex)
    {
        _mutex = xSemaphoreCreateMutex();
    }

    if (!lock())
        return false;
    _mounted = false;
    _ok = _mounted = ensureCardReady();
    release();

    notify(_mounted ? SdwEvent::CardMounted : SdwEvent::Error);
    return _mounted;
}

// Volat pod lockem
bool Sdw::reopenFileForWriting(void)
{
    if (_is_open)
        return true;
    if (_filename.length() == 0)
        return false;
    _file = SD.open(_filename, FILE_APPEND);
    _is_open = (bool)_file;
    return _is_open;
}

// Volat pod lockem
bool Sdw::ensureFileOpenForWriting(void)
{
    if (!ensureCardReady())
        return false;
    return reopenFileForWriting();
}

void Sdw::openNewFile(const String &filename)
{
    if (!_ok)
        return;
    if (!lock())
        return;
    bool wasopen = _is_open;
    if (_is_open)
    {
        _file.close();
        _is_open = false;
    }

    bool wasmntd = _mounted;
    bool mounted = ensureCardReady();

    bool is_open = false;
    if (mounted)
    {
        _filename = filename;
        if (SD.exists(_filename))
        {
            SD.remove(_filename);
        }
        _file = SD.open(_filename, FILE_WRITE);
        is_open = (bool)_file;
        _is_open = is_open;
    }
    release();

    if (wasopen)
        notify(SdwEvent::FileClosed);
    if (mounted)
        notify(is_open ? SdwEvent::FileOpened : SdwEvent::Error);
    else
        notify(wasmntd ? SdwEvent::CardRemoved : SdwEvent::Error);
}

uint64_t Sdw::size(void)
{
    if (!_ok)
        return 0;
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    uint64_t size = mounted ? SD.totalBytes() : 0;
    release();
    return size;
}

uint64_t Sdw::used(void)
{
    if (!_ok)
        return 0;
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    uint64_t size = mounted ? SD.usedBytes() : 0;
    release();
    return size;
}

uint64_t Sdw::free(void)
{
    if (_ok)
        return 0;
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    uint64_t size = 0;
    if (mounted)
    {
        uint64_t size_t = SD.totalBytes();
        uint64_t size_u = SD.usedBytes();
        size = (size_t > size_u) ? (size_t - size_u) : 0;
    }
    release();
    return size;
}