#pragma once

#include <Arduino.h>

/*
https://dratek.cz/arduino-platforma/1261-rtc-hodiny-realneho-casu-ds3231-at24c32-iic-pametovy-modul-pro-arduino.html
https://navody.dratek.cz/navody-k-produktum/rtc-hodiny-realneho-casu-ds3231-at24c32-pametovy-modul.html
https://www.youtube.com/watch?v=E6wkvTG2Ofs
https://thecavepearlproject.org/2014/05/21/using-a-cheap-3-ds3231-rtc-at24c32-eeprom-from-ebay
DS3231 je low-cost, extrémně přesný I2C modul reálného času (RTC) s integrovaným teplotně kompenzovaným krystalovým oscilátorem (TCXO) a krystalem.

Použití v komerčních a průmyslových aplikacích 16-pin 300mil. RTC poskytuje sekundy, minuty, hodiny, den, datum, měsíc a rok.

Funkce: méně než 31 dnů, datum ukončení, korekce pro přestupný rok. AM / PM. Dva konfigurovatelné budíky a kalendář. Adresy a data jsou přenášena přes I2C sběrnici.

Parametry Modulu:

Velikost: 38 x 22 x 14 mm
Hmotnost: 8 gramů
Provozní napětí: 3,3 - 5,5V
Čip: vysoce přesné hodiny DS3231
Přesnost: 0 - 40 °C rozsah, přesnost 2 ppm
Paměťový čip: AT24C32 (kapacita úložiště 32 KB)
Rozhraní: IIC, maximální přenosová rychlost 400KHz (napětí 5V)
Lze navrstvit s jinými IIC zařízeními, 24 C 32 A0/A1/A2 výchozí adresa je 0 x 57
Baterie LIR2032: k zajištění pojistky při výpadku napájení.
Příklad pro zapojení (Arduino UNO R3):

SCL → A5
SDA → A4
VCC → 5V
GND → GND
*/
class Rtc
{
public:
    ~Rtc();
    bool begin(uint8_t i2c_addr = 0x57, uint8_t rtc_addr = 0x68);
    bool ok() const { return _ok; }

    void displayTime();

private:
    uint8_t _i2c_addr, _rtc_addr;
    bool _ok = false;

    byte bcdDec(byte val);
    byte decBcd(byte val);
    void setRtcTime(byte s, byte m, byte h,
                    byte D, byte d, byte M, byte Y);
    void getRtcTime(byte *s, byte *m, byte *h,
                    byte *D, byte *d, byte *M, byte *Y);
};