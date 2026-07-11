#include "nmea2000.h"

#include "esphome/core/log.h"

namespace esphome {
namespace nmea2000 {

static const char *const TAG = "nmea2000";

void Nmea2000Component::setup() {}

void Nmea2000Component::loop() {}

void Nmea2000Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NMEA2000:");
  ESP_LOGCONFIG(TAG, "  CAN TX Pin: GPIO%u", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  CAN RX Pin: GPIO%u", this->rx_pin_);
  ESP_LOGCONFIG(TAG, "  Bitrate: 250 kbit/s (fixed)");
  ESP_LOGCONFIG(TAG, "  Node: source=%u unique=%u manufacturer=%u function=%u class=%u",
                this->source_address_, this->unique_number_, this->manufacturer_code_, this->device_function_,
                this->device_class_);
  ESP_LOGCONFIG(TAG, "  Product: '%s' code=%u sw='%s'", this->product_name_.c_str(), this->product_code_,
                this->software_version_.c_str());
}

}  // namespace nmea2000
}  // namespace esphome
