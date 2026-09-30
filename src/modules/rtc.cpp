#include "rtc.h"

#include "WiFi.h"
#include <WiFiUdp.h>

bool Rtc::begin(uint8_t i2c_addr, uint8_t rtc_addr,
                TwoWire &wire)
{
    _i2c_addr = i2c_addr;
    _rtc_addr = rtc_addr;
    wire.begin();

    wire.beginTransmission(i2c_addr);
    bool is_i2c = (wire.endTransmission() == 0);
    wire.beginTransmission(rtc_addr);
    bool is_rtc = (wire.endTransmission() == 0);
    Serial.printf("RTC init result: is_i2c(0x%02X) = %d, is_rtc(0x%02X) = %d\n",
                  i2c_addr, is_i2c, rtc_addr, is_rtc);
    _ok = _rtc.begin(&wire);
    if (!_ok)
        return false;
    _rtc.disable32K();
    _lostPower = _rtc.lostPower();
    if (_lostPower)
    {
        Serial.println("! RTC lost energy");
        // _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return true;
}

bool Rtc::now(DateTime &dt)
{
    if (!_ok)
    {
        dt = DateTime(time(nullptr));
        return false;
    }
    dt = _rtc.now();
    return dt.isValid();
}

DateTime Rtc::now(void)
{
    if (!_ok)
        return DateTime(time(nullptr));
    return _rtc.now();
}

float Rtc::temperature(void)
{
    if (!_ok)
        return NAN;
    return _rtc.getTemperature();
}

namespace rtc_wifi
{
    const uint32_t WIFI_TIMEOUT_MS = 5000;

    bool is_wifi(void)
    {
        return WiFi.getMode() != WIFI_OFF || WiFi.status() == WL_CONNECTED;
    }

    bool is_ssid_avaliable(const char *ssid)
    {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        int n = WiFi.scanNetworks();
        bool found = false;
        for (int i = 0; i < n; i++)
        {
            if (WiFi.SSID(i) == ssid)
            {
                found = true;
                break;
            }
        }
        WiFi.scanDelete();
        return found;
    }

    bool connect(const char *ssid, const char *pass,
                 uint32_t timeout = 5000)
    {
        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);
        WiFi.mode(WIFI_STA);
        delay(50);
        Serial.printf("RTC wifi: connecting to '%s'\n", ssid);
        WiFi.begin(ssid, pass);
        uint32_t t0 = millis();
        wl_status_t wl;
        while ((wl = WiFi.status()) != WL_CONNECTED && millis() - t0 < timeout)
        {
            if (wl == WL_NO_SSID_AVAIL)
            {
                Serial.printf("! RTC wifi: '%s' not found\n", ssid);
                return false;
            }
            delay(100);
        }
        Serial.println();
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("! RTC wifi failed");
            return false;
        }
        Serial.printf("RTC wifi: connected, IP = %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    void disconnect(void)
    {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
}

namespace
{
    const uint16_t NTP_PORT = 123;
    const uint16_t NTP_PORT_LOCAL = 2390;
    const uint32_t NTP_UNIX_OFFSET = 2208988800UL;
    const uint8_t NTP_RETRIES = 3;

    uint32_t readBE32(const uint8_t *p)
    {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }

    bool ntpQuery(WiFiUDP &udp, IPAddress ip, uint32_t timeout,
                  uint32_t &out_s, uint16_t &out_m)
    {
        uint8_t pkt[48] = {0};
        pkt[0] = 0b00100011;

        uint32_t nonce = esp_random();
        pkt[40] = nonce >> 24;
        pkt[41] = nonce >> 16;
        pkt[42] = nonce >> 8;
        pkt[43] = nonce;

        while (udp.parsePacket() > 0)
            udp.flush();

        uint32_t t0 = millis();
        udp.beginPacket(ip, NTP_PORT);
        udp.write(pkt, sizeof(pkt));

        if (!udp.endPacket())
            return false;

        while (millis() - t0 < timeout)
        {
            if (udp.parsePacket() >= 48)
            {
                uint32_t rtt = millis() - t0;
                udp.read(pkt, 48);

                uint8_t mode = pkt[0] & 0x07;
                uint8_t li = pkt[0] >> 6;
                uint8_t stratum = pkt[1];

                if (mode != 4 || li == 3 || stratum == 0 || stratum > 15)
                    return false;
                if (readBE32(&pkt[24]) != nonce)
                    return false;

                uint32_t sec = readBE32(&pkt[40]);
                uint32_t fra = readBE32(&pkt[44]);
                if (sec < NTP_UNIX_OFFSET)
                    return false;

                uint64_t ms = (uint64_t)(sec - NTP_UNIX_OFFSET) * 1000ULL + (((uint64_t)fra * 1000ULL) >> 32) + rtt / 2;
                out_s = ms / 1000;
                out_m = ms % 1000;

                Serial.printf("RTC init: stratum %u, rtt %lu ms\n", stratum, (unsigned long)rtt);
                return true;
            }
            delay(10);
        }
        return false;
    }

    bool ntpFetch(const char *host, uint32_t timeout, DateTime &out)
    {
        out = DateTime(time(nullptr));
        IPAddress ip;
        if (!WiFi.hostByName(host, ip))
        {
            Serial.printf("RTC init: DNS failed '%s'\n", host);
            return false;
        }
        WiFiUDP udp;
        if (!udp.begin(NTP_PORT_LOCAL))
            return false;
        bool ok = false;
        uint32_t s = 0;
        uint16_t m = 0;
        for (uint8_t i = 0; i < NTP_RETRIES && !ok; i++)
            ok = ntpQuery(udp, ip, timeout, s, m);
        udp.stop();
        if (!ok)
            return false;
        delay(1000 - m);
        time_t t = s + 1;
        struct tm lt;
        localtime_r(&t, &lt);
        out = DateTime(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                       lt.tm_hour, lt.tm_min, lt.tm_sec);
        return true;
    }
}

bool Rtc::adjust(const DateTime &dt)
{
    if (!_ok)
        return false;
    _rtc.adjust(dt);
    _lostPower = false;
    return true;
}

bool Rtc::adjust(const char *ssid, const char *pass, uint32_t timeout)
{
    if (!_ok)
        return false;
    bool ok = false;
    if (rtc_wifi::connect(ssid, pass, timeout))
    {
        DateTime t;
        if (ntpFetch("pool.ntp.org", timeout, t))
            ok = adjust(t);
    }
    rtc_wifi::disconnect();
    return ok;
}