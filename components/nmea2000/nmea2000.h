#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"

#include "nmea2000_twai.h"
#include "nmea2000_sensor.h"

#include <string>
#include <vector>

namespace esphome {
namespace nmea2000 {

// Values match tN2kDCType (N2kTypes.h)
enum class DcType : uint8_t {
  BATTERY = 0,
  ALTERNATOR = 1,
  CONVERTER = 2,
  SOLAR_CELL = 3,
  WIND_GENERATOR = 4,
};

// Values match tN2kTempSource (N2kTypes.h)
enum class TempSource : uint8_t {
  SEA = 0,
  OUTSIDE = 1,
  INSIDE = 2,
  ENGINE_ROOM = 3,
  MAIN_CABIN = 4,
  LIVE_WELL = 5,
  BAIT_WELL = 6,
  REFRIGERATION = 7,
  HEATING_SYSTEM = 8,
  DEW_POINT = 9,
  APPARENT_WIND_CHILL = 10,
  THEORETICAL_WIND_CHILL = 11,
  HEAT_INDEX = 12,
  FREEZER = 13,
  EXHAUST_GAS = 14,
  SHAFT_SEAL = 15,
};

// Values match tN2kHumiditySource (N2kTypes.h)
enum class HumiditySource : uint8_t {
  INSIDE = 0,
  OUTSIDE = 1,
};

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

  void add_battery_status(uint32_t interval_ms, uint8_t instance, sensor::Sensor *voltage, sensor::Sensor *current,
                          sensor::Sensor *temperature);
  void add_dc_detailed_status(uint32_t interval_ms, uint8_t instance, DcType dc_type, sensor::Sensor *state_of_charge,
                              sensor::Sensor *time_remaining);
  void add_temperature(uint32_t interval_ms, uint8_t instance, TempSource source, sensor::Sensor *actual);
  void add_humidity(uint32_t interval_ms, uint8_t instance, HumiditySource source, sensor::Sensor *actual);

  void register_sensor(Nmea2000Sensor *sens) { this->sensors_.push_back(sens); }

  // Called from the tNMEA2000 message-handler trampoline; not part of the public API.
  void handle_message_(const tN2kMsg &msg);

 protected:
  enum class PgnKind : uint8_t {
    BATTERY_STATUS,      // PGN 127508, single frame
    DC_DETAILED_STATUS,  // PGN 127506, fast packet
    TEMPERATURE,         // PGN 130312
    HUMIDITY,            // PGN 130313
  };

  struct TransmitEntry {
    PgnKind kind;
    uint32_t interval_ms;
    uint8_t instance;
    uint8_t enum_value;  // DcType / TempSource / HumiditySource
    // Slot meaning by kind: battery_status: voltage/current/temperature,
    // dc_detailed_status: state_of_charge/time_remaining/-, temperature+humidity: actual/-/-
    sensor::Sensor *a{nullptr};
    sensor::Sensor *b{nullptr};
    sensor::Sensor *c{nullptr};
    uint32_t last_send{0};
  };

  void process_transmit_entries_();
  void send_entry_(TransmitEntry &entry);
  void check_bus_status_();
  void publish_(SensorType type, float value);
  void publish_wind_(SensorType type, uint8_t reference, float value);

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

  std::vector<TransmitEntry> transmit_entries_;
  std::vector<Nmea2000Sensor *> sensors_;

  Nmea2000Twai *n2k_{nullptr};
  ESPPreferenceObject source_pref_;
  uint32_t last_bus_check_{0};
  bool error_passive_logged_{false};
};

}  // namespace nmea2000
}  // namespace esphome
