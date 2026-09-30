#include "sdw.h"

#include <sys/stat.h>

#ifndef SDW_MOUNTPOINT
#define SDW_MOUNTPOINT "/sd"
#endif

namespace
{
    constexpr uint32_t RETRY_MIN_MS = 1000;
    constexpr uint32_t RETRY_MAX_MS = 10000;

    enum class FileOp : uint8_t
    {
        WriteOk,
        FileHandleFailed,
        WriteFailed
    };

    FileOp
    writeFile(const char *path, const char *mode, const char *line)
    {
        FILE *f = fopen(path, mode);
        if (!f)
            return FileOp::FileHandleFailed;
        bool ok = true;
        if (line)
            ok = fputs(line, f) >= 0 && fputs("\r\n", f) >= 0;
        ok = ok && fflush(f) == 0;
        ok = ok && fsync(fileno(f)) == 0;

        ok = (fclose(f) == 0) && ok;
        return ok ? FileOp::WriteOk : FileOp::WriteFailed;
    }
}

bool Sdw::lock(uint32_t timeout)
{
    if (!_useMutex || !_mutex)
        return true;
    return xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeout)) == pdTRUE;
}

void Sdw::release()
{
    if (_useMutex && _mutex)
        xSemaphoreGive(_mutex);
}

Sdw::~Sdw()
{
    if (lock())
    {
        if (_mounted)
        {
            SD.end();
            _mounted = false;
        }
        release();
        if (_mutex)
            vSemaphoreDelete(_mutex);
    }
}

void Sdw::notify(SdwEvent event)
{
    _last = event;
    if (_callback)
        _callback(event);
}

// Volat BEZ lock()
void Sdw::emit(const Event &e)
{
    for (uint8_t i = 0; i < e.n; i++)
        notify(e.event[i]);
}

// Volat POD lock()
bool Sdw::ensureCardReady(Event &e)
{
    if (_mounted)
        return true;
    if (_retryTo != 0 && (int32_t)(millis() - _retryAt) < 0)
    {
        e.add(SdwEvent::NotReady);
        return false;
    }
    SD.end();
    _mounted = SD.begin(_csPin, SPI, _freqHz, SDW_MOUNTPOINT);
    if (_mounted)
    {
        _retryTo = 0;
        e.add(SdwEvent::CardMounted);
    }
    else
    {
        uint32_t to = _retryTo * 2;
        if (_retryTo == 0)
            _retryTo = RETRY_MIN_MS;
        else
            _retryTo = (to > RETRY_MAX_MS) ? RETRY_MAX_MS : to;
        _retryAt = millis() + _retryTo;
        e.add(SdwEvent::MountFailed);
    }
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
        if (!_mutex)
            notify(SdwEvent::Error);
    }

    Event e;
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return false;
    }
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    SPI.begin(_clPin, _soPin, _siPin, _csPin);
    _retryTo = 0;

    bool mounted = ensureCardReady(e);
    release();
    emit(e);
    return mounted;
}

// Volat POD lock()
void Sdw::cardLost(Event &e)
{
    SD.end();
    _mounted = false;
    _retryTo = 0;
    e.add(SdwEvent::CardRemoved);
}

// Volat POD lock()
void Sdw::handleFailure(Event &e, SdwEvent event)
{
    uint64_t size = SD.totalBytes();
    if (size == 0)
    {
        cardLost(e);
        return;
    }
    uint64_t used = SD.usedBytes();
    if (used + 64ULL * 1024ULL >= size)
        e.add(SdwEvent::CardFull);
    else
        e.add(event);
}

// Volat POD lock()
bool Sdw::getSpace(Event &e, uint64_t &size, uint64_t &used)
{
    size = 0;
    used = 0;
    if (!ensureCardReady(e))
        return false;
    size = SD.totalBytes();
    if (size == 0)
    {
        cardLost(e);
        return false;
    }
    used = SD.usedBytes();
    return true;
}

bool Sdw::openNewFile(const String &filename)
{
    Event e;
    if (!lock())
    {
        e.add(SdwEvent::Busy);
        e.add(SdwEvent::FileHandleFailed);
        emit(e);
        return false;
    }

    if (_filepath.length() > 0)
        e.add(SdwEvent::FileClosed);
    _filename = "";
    _filepath = "";
    _truncate = false;

    bool ok = false;
    if (filename.length() == 0)
    {
        e.add(SdwEvent::Error);
    }
    else
    {
        if (filename.startsWith("/"))
            _filename = filename;
        else
            _filename = String("/") + filename;
        _filepath = String(SDW_MOUNTPOINT) + _filename;
        _truncate = true;

        if (ensureCardReady(e))
        {
            FileOp f = writeFile(_filepath.c_str(), "w", nullptr);
            ok = (f == FileOp::WriteOk);
            if (ok)
            {
                _truncate = false;
                e.add(SdwEvent::FileOpened);
            }
            else
            {
                handleFailure(e, SdwEvent::FileHandleFailed);
            }
        }
    }
    release();
    emit(e);
    return ok;
}

void Sdw::close_file(void)
{
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return;
    }

    bool ok = _filepath.length() > 0;
    _filename = "";
    _filepath = "";
    _truncate = false;

    release();
    if (ok)
        notify(SdwEvent::FileClosed);
}

bool Sdw::writeLine(const String &line)
{
    return writeLine(line.c_str());
}

bool Sdw::writeLine(const char *line)
{
    Event e;
    bool ok = false;
    if (!lock())
    {
        e.add(SdwEvent::Busy);
    }
    else
    {
        if (_filepath.length() == 0)
        {
            e.add(SdwEvent::NoFile);
        }
        else if (ensureCardReady(e))
        {
            bool truncate = _truncate;
            FileOp f = writeFile(_filepath.c_str(), truncate ? "w" : "a", line ? line : "");
            ok = (f == FileOp::WriteOk);
            if (ok && truncate)
            {
                _truncate = false;
                e.add(SdwEvent::FileOpened);
            }
            else if (!ok)
            {
                handleFailure(e, f == FileOp::FileHandleFailed
                                     ? SdwEvent::FileHandleFailed
                                     : SdwEvent::Error);
            }
        }
        release();
    }
    e.add(ok ? SdwEvent::WriteOk : SdwEvent::WriteError);
    emit(e);
    return ok;
}

String Sdw::cardType()
{
    Event e;
    String type = "None";
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return type;
    }
    if (ensureCardReady(e))
    {
        switch (SD.cardType())
        {
        case CARD_NONE:
            type = "None";
            break;
        case CARD_MMC:
            type = "MMC";
            break;
        case CARD_SD:
            type = "SDSC";
            break;
        case CARD_SDHC:
            type = "SDHC/SDXC";
            break;
        default:
            type = "Unknown";
            break;
        }
    }
    release();
    emit(e);
    return type;
}

uint64_t Sdw::size(void)
{
    Event e;
    uint64_t size = 0, used = 0;
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return 0;
    }
    getSpace(e, size, used);
    release();
    emit(e);
    return size;
}

uint64_t Sdw::size_file(void)
{
    uint64_t sz = 0;
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return 0;
    }
    struct stat st;
    if (_mounted && _filepath.length() > 0 && stat(_filepath.c_str(), &st) == 0)
        sz = (uint64_t)st.st_size;
    release();
    return sz;
}

uint64_t Sdw::used(void)
{
    Event e;
    uint64_t size = 0, used = 0;
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return 0;
    }
    getSpace(e, size, used);
    release();
    emit(e);
    return used;
}

float Sdw::usedPercent(void)
{
    Event e;
    uint64_t size = 0, used = 0;
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return 0.0f;
    }
    bool ok = getSpace(e, size, used);
    release();
    emit(e);
    return ok ? (float)((double)used * 100.0 / (double)size) : 0.0f;
}

uint64_t Sdw::free(void)
{
    Event e;
    uint64_t size = 0, used = 0;
    if (!lock())
    {
        notify(SdwEvent::Busy);
        return 0;
    }
    getSpace(e, size, used);
    release();
    emit(e);
    return (size > used) ? (size - used) : 0;
}

bool Sdw::ok(void)
{
    Event e;
    bool ok = false;
    if (!lock())
    {
        e.add(SdwEvent::Busy);
    }
    else
    {
        if (ensureCardReady(e))
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)millis());
            FileOp f = writeFile(SDW_MOUNTPOINT "/.sdiot", "w", buf);
            ok = (f == FileOp::WriteOk);
            if (!ok)
                handleFailure(e, SdwEvent::Error);
        }
        release();
    }
    emit(e);
    return ok;
}

String Sdw::filename(void)
{
    String f;
    if (lock())
    {
        f = _filename;
        release();
    }
    return f;
}

String Sdw::filepath(void)
{
    String f;
    if (lock())
    {
        f = _filepath;
        release();
    }
    return f;
}