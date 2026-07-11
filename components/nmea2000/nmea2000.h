#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include "nmea2000_twai.h"

#include <string>

namespace esphome {
namespace nmea2000 {

class Nmea2000Component : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_can_pins(uint8_t tx_pin, uint8_t rx_pin) {
    this->tx_pin_ = tx_pin;
    this->rx_pin_ = rx_pin;
  }
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_unique_number(uint32_t unique_number) { this->unique_number_ = unique_number; }
  void set_manufacturer_code(uint16_t manufacturer_code) { this->manufacturer_code_ = manufacturer_code; }
  void set_device_function(uint8_t device_function) { this->device_function_ = device_function; }
  void set_device_class(uint8_t device_class) { this->device_class_ = device_class; }
  void set_product_name(const std::string &product_name) { this->product_name_ = product_name; }
  void set_product_code(uint16_t product_code) { this->product_code_ = product_code; }
  void set_software_version(const std::string &software_version) { this->software_version_ = software_version; }

 protected:
  void check_bus_status_();

  uint8_t tx_pin_{6};
  uint8_t rx_pin_{7};

  uint8_t source_address_{15};
  uint32_t unique_number_{1};
  uint16_t manufacturer_code_{2046};
  uint8_t device_function_{130};
  uint8_t device_class_{25};
  std::string product_name_{"ESPHome NMEA2000"};
  uint16_t product_code_{100};
  std::string software_version_{"1.0.0"};

  Nmea2000Twai *n2k_{nullptr};
  ESPPreferenceObject source_pref_;
  uint32_t last_bus_check_{0};
  bool error_passive_logged_{false};
};

}  // namespace nmea2000
}  // namespace esphome
