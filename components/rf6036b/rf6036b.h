#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace rf6036b {

// Ein einzelner Kanal-Sensor (Temperatur + Feuchte für einen Kanal)
struct Rf6036bChannel {
  uint8_t channel;                  // Kanal 1-3
  sensor::Sensor *temperature{nullptr};
  sensor::Sensor *humidity{nullptr};
};

class Rf6036bComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  // Wird vom Python-Code während der Konfiguration aufgerufen
  void set_receiver(remote_base::RemoteReceiverBase *receiver) { this->receiver_ = receiver; }

  void add_channel(uint8_t channel, sensor::Sensor *temperature, sensor::Sensor *humidity) {
    this->channels_.push_back({channel, temperature, humidity});
  }

  // Component-Lifecycle
  void setup() override {
    this->receiver_->register_listener(this);
  }

  void dump_config() override;

  float get_setup_priority() const override {
    // Später als remote_receiver initialisieren
    return setup_priority::DATA;
  }

  // RemoteReceiverListener Interface - wird bei jedem empfangenen Burst aufgerufen
  bool on_receive(remote_base::RemoteReceiveData data) override;

 protected:
  remote_base::RemoteReceiverBase *receiver_{nullptr};
  std::vector<Rf6036bChannel> channels_;

  // Dekodiert einen einzelnen RawTimings-Burst in ein 36-Bit-Frame.
  // Gibt true zurück wenn ein gültiges Frame gefunden wurde.
  bool decode_frame_(const RawTimings &raw, uint8_t &out_channel, float &out_temp, uint8_t &out_humidity,
                     uint8_t &out_id, bool &out_battery_ok);
};

}  // namespace rf6036b
}  // namespace esphome
