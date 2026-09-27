#pragma once

#include "sdw.h"

#ifndef SDW_MOUNTPOINT
#define SDW_MOUNTPOINT "/sd"
#endif

namespace sdw_test
{
    const char *sdwEventName(SdwEvent event)
    {
        switch (event)
        {
        case SdwEvent::None:
            return "None";
        case SdwEvent::Error:
            return "Error";
        case SdwEvent::CardMounted:
            return "CardMounted";
        case SdwEvent::CardRemoved:
            return "CardRemoved";
        case SdwEvent::FileOpened:
            return "FileOpened";
        case SdwEvent::FileClosed:
            return "FileClosed";
        case SdwEvent::WriteOk:
            return "WriteOk";
        case SdwEvent::WriteError:
            return "WriteError";
        case SdwEvent::MountFailed:
            return "MountFailed";
        case SdwEvent::NotReady:
            return "NotReady";
        case SdwEvent::FileHandleFailed:
            return "FileHandleFailed";
        case SdwEvent::NoFile:
            return "NoFile";
        case SdwEvent::CardFull:
            return "CardFull";
        case SdwEvent::Busy:
            return "Busy";
        }
        return "Unknown";
    }

    inline void makeBuf(char *buf, size_t len, int i)
    {
        snprintf(buf, len, "line %04d;%lu;%.*s", i, (unsigned long)(i * 7919UL),
                 (i * 13) % 60, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    }

    inline bool testSdw(Stream &out, bool test, const char *line, int &error)
    {
        out.printf("  [%s] %s\n", test ? " OK " : "FAIL", line);
        if (!test)
            error++;
        return test;
    }
}

inline bool sdwSelfTest(Sdw &sdw, Stream &out, int n = 50)
{
    using namespace sdw_test;

    const char *filename = "/.sd";
    const char *filepath = SDW_MOUNTPOINT "/.sd";

    char buf[128];
    char msg[128];

    int err = 0;

    out.println("SDW test");
    if (!testSdw(out, sdw.ok(), "ok()", err))
    {
        out.printf("  last: %s\n", sdwEventName(sdw.last()));
        return false;
    }
    out.printf("  type: %s, %llu MB, free %llu MB\n", sdw.cardType().c_str(),
               (unsigned long long)(sdw.size() / (1024ULL * 1024ULL)),
               (unsigned long long)(sdw.free() / (1024ULL * 1024ULL)));
    testSdw(out, sdw.openNewFile(filename), "openNewFile()", err);
    testSdw(out, sdw.size_file() == 0, "size_file() == 0", err);

    uint32_t tmin = UINT32_MAX, tmax = 0, tsum = 0;
    uint64_t size = 0;
    int wrt = 0;
    for (int i = 0; i < n; i++)
    {
        makeBuf(buf, sizeof(buf), i);
        uint32_t t0 = millis();
        bool ok = sdw.writeLine(buf);
        uint32_t dt = millis() - t0;
        if (ok)
            size += strlen(buf) + 2; // \r\n
        else
        {
            wrt++;
            out.printf("  i:%d failed, event: %s\n", i, sdwEventName(sdw.last()));
        }
        tsum += dt;
        if (dt < tmin)
            tmin = dt;
        if (dt > tmax)
            tmax = dt;
    }
    snprintf(msg, sizeof(msg), "writeLine(): ok %d of %d lines", n - wrt, n);
    testSdw(out, wrt == 0, msg, err);
    out.printf("  time: min %lu ms, avg %lu ms, max %lu ms\n",
               (unsigned long)tmin, (unsigned long)(n ? tsum / n : 0), (unsigned long)tmax);
    uint64_t sz = sdw.size_file();
    snprintf(msg, sizeof(msg), "size_file(): %llu B (expected %llu B)",
             (unsigned long long)sz, (unsigned long long)size);
    testSdw(out, sz == size, msg, err);
    FILE *f = fopen(filepath, "r");
    if (testSdw(out, f != nullptr, "fopen()", err))
    {
        int i = 0, e = 0;
        char str[256];
        while (fgets(str, sizeof(str), f))
        {
            str[strcspn(str, "\r\n")] = 0;
            makeBuf(buf, sizeof(buf), i);
            if (strcmp(str, buf) != 0)
            {
                if (e < 3)
                    out.printf("  i:%d, read '%s', expected '%s'\n", i, str, buf);
                e++;
            }
            i++;
        }
        fclose(f);
        snprintf(msg, sizeof(msg), "fgets(): i:%d, %d different", i, e);
        testSdw(out, i == n - wrt && e == 0, msg, err);
    }
    sdw.openNewFile(filename);
    testSdw(out, sdw.size_file() == 0, "size_file() == 0", err);
    sdw.writeLine("writeLine()");
    testSdw(out, sdw.size_file() == strlen("writeLine()\r\n"), "size_file()", err);
    sdw.close_file();
    bool is_open = sdw.writeLine("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    testSdw(out, !is_open && sdw.last() == SdwEvent::WriteError, "close_file()->writeLine()", err);
    return err == 0;
}