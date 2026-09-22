#pragma once

#include <Arduino.h>
#include <functional>
#include "SPI.h"
#include "SD.h"

enum class SdwEvent : uint8_t
{
    None,
    Error,
    CardMounted,
    CardRemoved,
    FileOpened,
    FileClosed,
    WriteOk,
    WriteError,
};

using SdwEventCallback = std::function<void(SdwEvent)>;

class Sdw
{
public:
    ~Sdw();
    bool begin(uint8_t csPin = 5, uint8_t clPin = -1, uint8_t soPin = -1, uint8_t siPin = -1,
               bool useMutex = true, uint32_t freqHz = 4000000);

    bool ok() const { return _ok; };

    void flush_file();
    void close_file();

    bool mounted() const { return _mounted; };
    bool is_open() const { return _is_open; }

    String cardType(void);
    uint64_t size(void);
    uint64_t used(void);
    float usedPercent(void);
    uint64_t free(void);

    bool test(void);

    void call(SdwEventCallback callback) { _callback = callback; }
    SdwEvent last() const { return _last; }

    void openNewFile(const String &filename);

    bool writeLine(const String &line);
    bool writeLine(const char *line);

    void listDir(const char *dirPath = "/", bool recursive = true, Stream &out = Serial);

private:
    uint8_t _csPin = 5, _clPin = -1, _soPin = -1, _siPin = -1;
    uint32_t _freqHz = 4000000;
    bool _ok = false;

    bool _is_open = false;
    bool _mounted = false;

    uint32_t countFiles(const char *dirPath = "/", bool recursive = true);
    uint32_t countFilesRecursive(File dir, bool recursive);

    SdwEventCallback _callback = nullptr;
    SdwEvent _last = SdwEvent::None;
    void notify(SdwEvent event);

    File _file;
    String _filename;
    bool _useMutex;
    SemaphoreHandle_t _mutex = nullptr;

    bool lock(uint32_t timeout = 1000);
    void release(void);

    bool ensureCardReady(void);
    bool ensureFileOpenForWriting(void);
    bool reopenFileForWriting(void);
};