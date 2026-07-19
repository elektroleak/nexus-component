# esphome-rf6036b

ESPHome External Component für den **RF6036B 433 MHz Temperatur- und Luftfeuchtigkeitssensor**
(und alle baugleichen Nexus-Protokoll-Sensoren, z. B. Digoo DG-R8H, FreeTec NC-7345, TFA 30.3209).

## Protokoll

Das Protokoll wurde durch Reverse Engineering aus realen Telegrammen ermittelt und entspricht
dem von `rtl_433` als **Protokoll #19 "Nexus-TH"** bekannten Format:

- **36 Bit** pro Telegramm, 8–9× wiederholt, getrennt durch Sync-Puls (~3900 µs)
- Modulation: OOK/PPM – fester HIGH-Puls (~500 µs) + variabler LOW-Puls
  - LOW ~1000 µs = Bit „0"
  - LOW ~1950 µs = Bit „1"

```
Bit  35..28  ID          (8 Bit)   – ändert sich bei Batteriewechsel
Bit  27      Batterie    (1 Bit)   – 1 = OK
Bit  26..25  Kanal       (2 Bit)   – 00=1, 01=2, 10=3
Bit  24      unbekannt   (1 Bit)
Bit  23..12  Temperatur  (12 Bit)  – signed, ÷10 → °C
Bit  11..8   Fix         (4 Bit)   – immer 0xF
Bit   7..0   Luftfeuchte (8 Bit)   – direkt in %
```

## Voraussetzungen

- ESP32 oder ESP8266
- Einfaches 433-MHz-Empfängermodul (z. B. RXB6, SRX882, XY-MK-5V)
- ESPHome ≥ 2025.1

## Installation

```yaml
external_components:
  - source: github://dein-github-user/esphome-rf6036b
    components: [rf6036b]
```

## Konfiguration

```yaml
remote_receiver:
  id: rf_receiver
  pin: GPIO27
  dump: all       # optional, für Tests
  tolerance: 50%
  filter: 100us
  idle: 4500us    # wichtig: muss größer als der Sync-Puls (~3950 µs) sein

rf6036b:
  id: nexus_receiver
  receiver_id: rf_receiver

sensor:
  - platform: rf6036b
    rf6036b_id: nexus_receiver
    channel: 1
    temperature:
      name: "Außen Temperatur"
    humidity:
      name: "Außen Luftfeuchte"

  - platform: rf6036b
    rf6036b_id: nexus_receiver
    channel: 2
    temperature:
      name: "Keller Temperatur"
    humidity:
      name: "Keller Luftfeuchte"
```

### Parameter

| Parameter | Beschreibung |
|-----------|-------------|
| `receiver_id` | ID des `remote_receiver` den die Component belauschen soll |
| `channel` | Kanal des Sensors (1–3, entspricht der Kanalwahl am Sensor) |
| `temperature` | Optionaler Temperatursensor |
| `humidity` | Optionaler Luftfeuchtesensor |

## Kompatibilität mit anderen RF-Geräten

Die Component registriert sich als passiver Listener auf dem `remote_receiver` und gibt **`false`**
aus `on_receive()` zurück. Dadurch sehen alle anderen konfigurierten Trigger, Dumper und
Protokoll-Decoder (RC-Switch, Pronto, Raw-Dump etc.) jeden empfangenen Burst weiterhin vollständig.

## Hinweise / Offene Punkte

- **Kanal-Kodierung** für Kanal 2 und 3 basiert auf der plausiblen Annahme `00=1, 01=2, 10=3`
  (analog zu rtl_433 `nexus.c`), wurde aber bisher nur mit Kanal 1 an echten Telegrammen
  verifiziert. Bitte ein Issue öffnen wenn Kanal 2/3 falsch erkannt wird.
- **Negative Temperaturen** sind implementiert (12-Bit-Zweierkomplement), aber noch nicht
  mit realen Telegrammen getestet.
- **Batterie-Status** wird dekodiert und geloggt, aber noch nicht als separater Sensor
  exponiert. PR willkommen.
