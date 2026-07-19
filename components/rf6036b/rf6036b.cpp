#include "rf6036b.h"
#include "esphome/core/log.h"
#include <map>
#include <cstdlib>

namespace esphome {
namespace rf6036b {

static const char *const TAG = "rf6036b";

static const int32_t SYNC_THRESHOLD_US = 3000;
static const int32_t BIT_THRESHOLD_US  = 1400;
static const uint8_t FRAME_BITS        = 36;
static const uint8_t MIN_VALID_FRAMES  = 3;

void Rf6036bComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RF6036B / Nexus-433 Sensor:");
  for (auto &ch : this->channels_) {
    ESP_LOGCONFIG(TAG, "  Kanal %d:", ch.channel);
    LOG_SENSOR("    ", "Temperatur", ch.temperature);
    LOG_SENSOR("    ", "Luftfeuchte", ch.humidity);
  }
}

bool Rf6036bComponent::decode_frame_(const remote_base::RawTimings &raw,
                                      uint8_t &out_channel, float &out_temp,
                                      uint8_t &out_humidity, uint8_t &out_id,
                                      bool &out_battery_ok) {
  std::map<uint64_t, int> frame_counts;
  uint64_t current_bits = 0;
  uint8_t bit_count = 0;

  for (size_t i = 0; i + 1 < raw.size(); i += 2) {
    int32_t low_us = std::abs((int32_t) raw[i + 1]);

    if (low_us > SYNC_THRESHOLD_US) {
      if (bit_count == FRAME_BITS)
        frame_counts[current_bits]++;
      current_bits = 0;
      bit_count = 0;
      continue;
    }
    if (bit_count < FRAME_BITS) {
      current_bits = (current_bits << 1) | (low_us >= BIT_THRESHOLD_US ? 1u : 0u);
      bit_count++;
    }
  }
  if (bit_count == FRAME_BITS)
    frame_counts[current_bits]++;

  if (frame_counts.empty())
    return false;

  uint64_t best_frame = 0;
  int best_count = 0;
  int total_frames = 0;
  for (auto &kv : frame_counts) {
    total_frames += kv.second;
    if (kv.second > best_count) {
      best_count = kv.second;
      best_frame = kv.first;
    }
  }

  if (best_count < MIN_VALID_FRAMES) {
    ESP_LOGV(TAG, "Zu wenig uebereinstimmende Frames (%d), verworfen", best_count);
    return false;
  }

  // Bit-Layout (36 Bit, MSB zuerst):
  //   [35:28] ID (8 Bit)
  //   [27]    Batterie OK (1 Bit)
  //   [26:25] Kanal 0-basiert (2 Bit)  00=CH1, 01=CH2, 10=CH3
  //   [24]    unbekannt (1 Bit)
  //   [23:12] Temperatur signed *10 (12 Bit)
  //   [11:8]  Fix 0xF (4 Bit)
  //   [7:0]   Luftfeuchte (8 Bit)
  out_id         = (uint8_t)  ((best_frame >> 28) & 0xFF);
  out_battery_ok = (bool)     ((best_frame >> 27) & 0x01);
  out_channel    = (uint8_t) (((best_frame >> 25) & 0x03) + 1);
  int16_t temp_raw = (int16_t) ((best_frame >> 12) & 0x0FFF);
  if (temp_raw & 0x0800)
    temp_raw |= (int16_t) 0xF000;  // Sign-Extension 12→16 Bit
  out_temp       = temp_raw / 10.0f;
  out_humidity   = (uint8_t)  (best_frame & 0xFF);

  if (out_temp < -40.0f || out_temp > 60.0f || out_humidity > 100) {
    ESP_LOGW(TAG, "Werte unplausibel (T=%.1f H=%d), verworfen", out_temp, out_humidity);
    return false;
  }

  ESP_LOGD(TAG, "ID=0x%02X Batt=%s Kanal=%d Temp=%.1f C Feuchte=%d%% (%d/%d Frames)",
           out_id, out_battery_ok ? "OK" : "LOW", out_channel,
           out_temp, out_humidity, best_count, total_frames);
  return true;
}

bool Rf6036bComponent::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t channel, id, humidity;
  float temperature;
  bool battery_ok;

  if (!this->decode_frame_(data.get_raw_data(), channel, temperature, humidity, id, battery_ok))
    return false;

  bool matched = false;
  for (auto &ch : this->channels_) {
    if (ch.channel == channel) {
      if (ch.temperature != nullptr)
        ch.temperature->publish_state(temperature);
      if (ch.humidity != nullptr)
        ch.humidity->publish_state((float) humidity);
      matched = true;
    }
  }
  if (!matched)
    ESP_LOGV(TAG, "Kanal %d empfangen, nicht konfiguriert", channel);

  return false;  // false = andere Listener/Dumper sehen den Burst weiterhin
}

}  // namespace rf6036b
}  // namespace esphome
