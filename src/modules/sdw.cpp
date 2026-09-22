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
    _mounted = SD.begin(_csPin, SPI, _freqHz);
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
    _mounted = ensureCardReady();
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

String Sdw::cardType(void)
{
    if (!lock())
        return "None";
    bool mounted = ensureCardReady();
    String type = "None";
    if (mounted)
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
    return type;
}

uint64_t Sdw::size(void)
{
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    uint64_t size = mounted ? SD.totalBytes() : 0;
    release();
    return size;
}

uint64_t Sdw::size_file(void)
{
    if (!lock())
        return 0;
    uint64_t size = _is_open ? _file.size() : 0;
    release();
    return size;
}

uint64_t Sdw::used(void)
{
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    uint64_t size = mounted ? SD.usedBytes() : 0;
    release();
    return size;
}

float Sdw::usedPercent(void)
{
    if (!lock())
        return 0.0f;
    bool mounted = ensureCardReady();
    float percent = 0.0f;
    if (mounted)
    {
        uint64_t size_t = SD.totalBytes();
        if (size_t > 0)
        {
            uint64_t size_u = SD.usedBytes();
            percent = (float)((double)size_u * 100.0 / (double)size_t);
        }
    }
    release();
    return percent;
}

uint64_t Sdw::free(void)
{
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

bool Sdw::test(void)
{
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    bool ok = false;
    if (mounted)
    {
        File file = SD.open("/.sdiot", FILE_WRITE);
        if (file)
        {
            size_t size = file.print(millis());
            file.flush();
            ok = (size > 0) && !file.getWriteError();
            file.close();
        }
    }
    release();
    return ok;
}

bool Sdw::ok(void)
{
    bool mounted = _mounted;
    bool ok = test();
    if (ok && !mounted)
        notify(SdwEvent::CardMounted);
    else if (!ok)
        notify(mounted ? SdwEvent::CardRemoved : SdwEvent::Error);
    return ok;
}

namespace
{
    void print(File &entry, Stream &out, int depth)
    {
        for (int i = 0; i < depth; i++)
            out.print("  ");
        if (entry.isDirectory())
        {
            out.println(entry.name());
        }
        else
        {
            out.print(entry.name());
            out.print("  (");
            out.print(entry.size());
            out.println(" B)");
        }
    }

    // Volat pod lockem
    uint32_t countFilesImpl(File dir, bool recursive)
    {
        uint32_t n = 0;
        File entry = dir.openNextFile();
        while (entry)
        {
            if (entry.isDirectory())
            {
                if (recursive)
                    n += countFilesImpl(entry, recursive);
            }
            else
                n++;
            entry.close();
            entry = dir.openNextFile();
        }
        return n;
    }

    void listDirImpl(File dir, bool recursive, Stream &out, int depth)
    {
        File entry = dir.openNextFile();
        while (entry)
        {
            print(entry, out, depth);
            if (entry.isDirectory() && recursive)
            {
                listDirImpl(entry, recursive, out, depth + 1);
            }
            entry.close();
            entry = dir.openNextFile();
        }
    }
}

void Sdw::listDir(const char *dirPath, bool recursive, Stream &out)
{
    if (!lock())
        return;
    bool mounted = ensureCardReady();
    bool is_open = false;
    if (mounted)
    {
        File dir = SD.open(dirPath);
        is_open = (bool)dir && dir.isDirectory();
        if (is_open)
        {
            listDirImpl(dir, recursive, out, 0);
            dir.rewindDirectory();
        }
        if (dir)
            dir.close();
    }
    release();
}

uint32_t Sdw::countFiles(const char *dirPath, bool recursive)
{
    if (!lock())
        return 0;
    bool mounted = ensureCardReady();
    uint32_t n = 0;
    if (mounted)
    {
        File dir = SD.open(dirPath);
        if (dir)
        {
            n = countFilesImpl(dir, recursive);
            dir.close();
        }
    }
    release();
    return n;
}

bool Sdw::writeLine(const String &line)
{
    return writeLine(line.c_str());
}

bool Sdw::writeLine(const char *line)
{
    if (!lock())
        return false;
    bool mounted = _mounted;
    bool is_open = ensureFileOpenForWriting();
    bool ok = false;
    if (is_open)
    {
        size_t size = _file.println(line);
        ok = size > 0;
        if (!ok)
        {
            _is_open = false;
            _mounted = false;
        }
    }
    release();
    if (!is_open)
        notify(mounted ? SdwEvent::CardRemoved : SdwEvent::Error);
    else
        notify(ok ? SdwEvent::WriteOk : SdwEvent::WriteError);
    return ok;
}