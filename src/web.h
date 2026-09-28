#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "modules/sdw.h"

class Web
{
public:
    explicit Web(uint16_t port = 80) : _server(port) {}
    void begin(Sdw &sdw, const char *ssid, const char *pass)
    {
        _sdw = &sdw;
        _ssid = ssid;
        _pass = pass;
        if (_routes)
            return;
        _routes = true;
        _server.on("/", HTTP_GET, [this]()
                   { _s0(); });
        _server.on("/view", HTTP_GET, [this]()
                   { _s1(false); });
        _server.on("/download", HTTP_GET, [this]()
                   { _s1(true); });
        _server.on("/delete", HTTP_POST, [this]()
                   { _s2(); });
        _server.onNotFound([this]()
                           { _server.send(404, "text/plain", "Not found"); });
    }

    bool ok() const { return _ok; }

    bool start(void)
    {
        if (_serves)
            return true;
        if (!_sdw)
            return false;
        const char *pass = (_pass && strlen(_pass) >= 8) ? _pass : nullptr;
        WiFi.mode(WIFI_AP);
        if (!WiFi.softAP(_ssid, pass))
        {
            WiFi.mode(WIFI_OFF);
            return false;
        }
        _server.begin();
        _serves = true;
        return _serves;
    }

    bool is_serving() const { return _serves; }
    void loop()
    {
        if (_serves)
            _server.handleClient();
    }

    void pause(void)
    {
        if (!_serves)
            return;
        _server.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        _serves = false;
    }

    const char *ssid() const { return _ssid; }
    const char *pass() const { return _pass; }
    IPAddress ip() const { return WiFi.softAPIP(); }

private:
    WebServer _server;
    bool _ok = false;

    Sdw *_sdw = nullptr;
    const char *_ssid = nullptr;
    const char *_pass = nullptr;

    bool _serves = false;
    bool _routes = false;

    void _s0(void);
    void _s1(bool mode);
    void _s2(void);
};