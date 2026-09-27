#pragma once

#include <Arduino.h>
#include "SPI.h"
#include "SD.h"

#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class SdwEvent : uint8_t
{
    None = 0,
    Error,            // Chyba
    CardMounted,      // Karta byla načtena správně
    CardRemoved,      // Karta přestala odpovídat
    FileOpened,       // Soubor byl otevřen
    FileClosed,       // Soubor byl zavřen
    WriteOk,          // Řádek je fyzicky na kartě
    WriteError,       // Přidání řádku selhalo
    MountFailed,      // Pokus o připojení karty selhal
    NotReady,         // Karta odpojená, čeká se na další pokus
    FileHandleFailed, // Soubor nejde otevřít/vytvořit
    NoFile,           // writeLine() bez předchozího openNewFile()
    CardFull,         // Na kartě došlo místo
    Busy              // Mutex se nepodařilo získat
};

using SdwEventCallback = std::function<void(SdwEvent)>;

class Sdw
{
public:
    ~Sdw();
    bool begin(uint8_t csPin = 5, uint8_t clPin = -1, uint8_t soPin = -1, uint8_t siPin = -1,
               bool useMutex = true, uint32_t freqHz = 1000000);
    bool ok(void);

    bool mounted() const { return _mounted; };

    bool openNewFile(const String &filename);
    void close_file(void);

    String cardType(void);
    uint64_t size(void);
    uint64_t size_file(void);
    uint64_t used(void);
    float usedPercent(void);
    uint64_t free(void);

    void onEvent(SdwEventCallback callback) { _callback = callback; }
    SdwEvent last() const { return _last; }

    bool writeLine(const String &line);
    bool writeLine(const char *line);

private:
    uint8_t _csPin = 5, _clPin = -1, _soPin = -1, _siPin = -1;
    uint32_t _freqHz = 1000000;

    static constexpr uint8_t MAX_EVENTS = 6;
    struct Event
    {
        SdwEvent event[MAX_EVENTS];
        uint8_t n = 0;
        void add(SdwEvent _event)
        {
            if (n < MAX_EVENTS)
                event[n++] = _event;
        }
    };

    bool lock(uint32_t timeout = 5000); // neúspěšný mount může trvat i sekundy
    void release(void);
    void notify(SdwEvent event);
    void emit(const Event &e);

    bool ensureCardReady(Event &e);
    void cardLost(Event &e);
    void handleFailure(Event &e, SdwEvent event);
    bool getSpace(Event &e, uint64_t &size, uint64_t &used);

    SemaphoreHandle_t _mutex = nullptr;
    bool _useMutex = true;
    SdwEventCallback _callback;
    SdwEvent _last = SdwEvent::None;

    String _filename;
    String _filepath;
    bool _truncate = false;

    bool _mounted = false;

    uint32_t _retryAt = 0;
    uint32_t _retryTo = 0;
};