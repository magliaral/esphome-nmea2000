#pragma once

#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace nmea2000 {

enum class SensorType : uint8_t {
  LATITUDE,             // PGN 129025
  LONGITUDE,            // PGN 129025
  SPEED_OVER_GROUND,    // PGN 129026
  COURSE_OVER_GROUND,   // PGN 129026, only published for reference "true"
  HEADING,              // PGN 127250
  WATER_DEPTH,          // PGN 128267, transducer offset applied
  SPEED_THROUGH_WATER,  // PGN 128259
  WIND_SPEED,           // PGN 130306
  WIND_ANGLE,           // PGN 130306
};

// Values match tN2kWindReference (N2kTypes.h)
enum class WindReference : uint8_t {
  TRUE_NORTH = 0,
  MAGNETIC = 1,
  APPARENT = 2,
  TRUE_BOAT = 3,
  TRUE_WATER = 4,
};

class Nmea2000Sensor : public sensor::Sensor {
 public:
  void set_type(SensorType type) { this->type_ = type; }
  void set_wind_reference(WindReference wind_reference) { this->wind_reference_ = wind_reference; }
  SensorType get_type() const { return this->type_; }
  WindReference get_wind_reference() const { return this->wind_reference_; }

 protected:
  SensorType type_{SensorType::LATITUDE};
  WindReference wind_reference_{WindReference::APPARENT};
};

}  // namespace nmea2000
}  // namespace esphome
