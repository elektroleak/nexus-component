#include "rf6036b.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rf6036b {

static const char *const TAG = "rf6036b";

// Protokoll-Timing-Konstanten (in Mikrosekunden, aus Telegrammanalyse)
static const int32_t SYNC_THRESHOLD_US  = 3000;  // LOW > 3000µs = Sync/Frame-Trenner
static const int32_t BIT_THRESHOLD_US   = 1400;  // LOW < 1400µs = "0", sonst "1"
static const uint8_t FRAME_BITS         = 36;
static const uint8_t MIN_VALID_FRAMES   = 3;      // Mindestanzahl übereinstimmender Frames

void Rf6036bComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RF6036B / Nexus-433 Sensor:");
  for (auto &ch : this->channels_) {
    ESP_LOGCONFIG(TAG, "  Kanal %d:", ch.channel);
    LOG_SENSOR("    ", "Temperatur", ch.temperature);
    LOG_SENSOR("    ", "Luftfeuchte", ch.humidity);
  }
}

bool Rf6036bComponent::decode_frame_(const RawTimings &raw, uint8_t &out_channel, float &out_temp,
                                      uint8_t &out_humidity, uint8_t &out_id, bool &out_battery_ok) {
  // --- Schritt 1: Bits aus Pulsbreiten extrahieren ---
  // Format: abwechselnd HIGH (positiv, ~500µs, konstant) und LOW (negativ, variabel)
  // Jedes Paar (HIGH, LOW) ergibt ein Bit: LOW kurz (~1000µs) = 0, LOW lang (~1950µs) = 1
  // Ein sehr langer LOW (>3000µs) ist der Sync-Puls zwischen den Frame-Wiederholungen.

  std::map<uint64_t, int> frame_counts; //test
  uint64_t current_frame_bits = 0;
  uint8_t current_bit_count = 0;

  for (size_t i = 0; i + 1 < raw.size(); i += 2) {
    // In ESPHome remote_receiver: positive Werte = HIGH (mark), negative = LOW (space)
    // Der erste Wert in einem Paar ist die HIGH-Phase, der zweite die LOW-Phase.
    // Da das Vorzeichen-Verhalten je nach Plattform variieren kann, arbeiten wir mit abs().
    int32_t high_us = std::abs((int32_t) raw[i]);
    int32_t low_us  = std::abs((int32_t) raw[i + 1]);

    if (low_us > SYNC_THRESHOLD_US) {
      // Sync-Puls: aktuelles Frame abschließen
      if (current_bit_count == FRAME_BITS) {
        frame_counts[current_frame_bits]++;
      }
      current_frame_bits = 0;
      current_bit_count = 0;
      continue;
    }

    if (current_bit_count < FRAME_BITS) {
      current_frame_bits = (current_frame_bits << 1) | (low_us >= BIT_THRESHOLD_US ? 1 : 0);
      current_bit_count++;
    }
  }
  // Letztes Frame (falls kein abschließender Sync-Puls)
  if (current_bit_count == FRAME_BITS) {
    frame_counts[current_frame_bits]++;
  }

  if (frame_counts.empty()) {
    return false;
  }

  // --- Schritt 2: Mehrheitsentscheid ---
  uint64_t best_frame = 0;
  int best_count = 0;
  for (auto &kv : frame_counts) {
    if (kv.second > best_count) {
      best_count = kv.second;
      best_frame = kv.first;
    }
  }

  if (best_count < MIN_VALID_FRAMES) {
    ESP_LOGV(TAG, "Zu wenig übereinstimmende Frames (%d), verworfen", best_count);
    return false;
  }

  // --- Schritt 3: Bitfelder extrahieren ---
  // Layout (36 Bit, MSB zuerst):
  //   [35:28] ID        (8 Bit)  – ändert sich bei Batteriewechsel
  //   [27]    Batterie  (1 Bit)  – 1 = OK, 0 = schwach
  //   [26:25] Kanal     (2 Bit)  – 00=Kanal1, 01=Kanal2, 10=Kanal3
  //   [24]    Unbekannt (1 Bit)  – immer 0 in allen beobachteten Telegrammen
  //   [23:12] Temperatur (12 Bit, vorzeichenbehaftet, ×10 skaliert)
  //   [11:8]  Fix-Bits  (4 Bit)  – immer 0xF in allen beobachteten Telegrammen
  //   [7:0]   Luftfeuchte (8 Bit)
  //
  // Hinweis zu Temperatur/Vorzeichen: Laut rtl_433 nexus.c ist das Feld signed 12-bit.
  // Bei positiven Temperaturen funktioniert einfache Integer-Division /10 korrekt.
  // Für negative Temperaturen wird Sign-Extension auf 32 Bit benötigt (siehe unten).

  out_id         = (best_frame >> 28) & 0xFF;
  out_battery_ok = (best_frame >> 27) & 0x01;
  out_channel    = ((best_frame >> 25) & 0x03) + 1;  // 0-basiert → 1-basiert
  int16_t temp_raw = (int16_t)((best_frame >> 12) & 0x0FFF);
  // Sign-Extension: 12-Bit Zweierkomplement → 16-Bit signed
  if (temp_raw & 0x0800) {
    temp_raw |= 0xF000;
  }
  out_temp     = temp_raw / 10.0f;
  out_humidity = (best_frame >> 0) & 0xFF;  // [7:0]

  // Plausibilitätscheck
  if (out_temp < -40.0f || out_temp > 60.0f || out_humidity > 100) {
    ESP_LOGW(TAG, "Werte außerhalb plausibler Grenzen (T=%.1f H=%d), verworfen",
             out_temp, out_humidity);
    return false;
  }

  ESP_LOGD(TAG, "Dekodiert: ID=0x%02X Batt=%s Kanal=%d Temp=%.1f°C Feuchte=%d%% (%d/%d Frames übereinstimmend)",
           out_id, out_battery_ok ? "OK" : "SCHWACH", out_channel,
           out_temp, out_humidity, best_count,
           (int)(best_count + frame_counts.size() - 1));

  return true;
}

bool Rf6036bComponent::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t channel, id, humidity;
  float temperature;
  bool battery_ok;

  if (!this->decode_frame_(data.get_raw_data(), channel, temperature, humidity, id, battery_ok)) {
    // Kein gültiges RF6036B-Frame → anderen Listenern/Dumpern die Chance lassen
    return false;
  }

  // Passendem Kanal die Werte zuweisen
  bool matched = false;
  for (auto &ch : this->channels_) {
    if (ch.channel == channel) {
      if (ch.temperature != nullptr) {
        ch.temperature->publish_state(temperature);
      }
      if (ch.humidity != nullptr) {
        ch.humidity->publish_state((float) humidity);
      }
      matched = true;
    }
  }

  if (!matched) {
    ESP_LOGV(TAG, "Kanal %d empfangen, aber nicht konfiguriert", channel);
  }

  // false zurückgeben damit dump: all und andere Listener den Burst ebenfalls sehen
  return false;
}

}  // namespace rf6036b
}  // namespace esphome
