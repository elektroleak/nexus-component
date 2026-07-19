#pragma once

#include <map>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace rf6036b {

struct Rf6036bChannel {
  uint8_t channel;
  sensor::Sensor *temperature{nullptr};
  sensor::Sensor *humidity{nullptr};
};

class Rf6036bComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  void set_receiver(remote_base::RemoteReceiverBase *receiver) { this->receiver_ = receiver; }

  void add_channel(uint8_t channel, sensor::Sensor *temperature, sensor::Sensor *humidity) {
    this->channels_.push_back({channel, temperature, humidity});
  }

  void setup() override { this->receiver_->register_listener(this); }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool on_receive(remote_base::RemoteReceiveData data) override;

 protected:
  remote_base::RemoteReceiverBase *receiver_{nullptr};
  std::vector<Rf6036bChannel> channels_;

  bool decode_frame_(const remote_base::RawTimings &raw, uint8_t &out_channel,
                     float &out_temp, uint8_t &out_humidity,
                     uint8_t &out_id, bool &out_battery_ok);
};

}  // namespace rf6036b
}  // namespace esphome
