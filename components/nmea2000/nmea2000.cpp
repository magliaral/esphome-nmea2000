#include "nmea2000.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <N2kMessages.h>

#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace nmea2000 {

static const char *const TAG = "nmea2000";

static const uint32_t BUS_CHECK_INTERVAL_MS = 2000;

// tNMEA2000::SetMsgHandler takes a plain function pointer, so the single
// component instance is dispatched through this trampoline.
static Nmea2000Component *g_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Missing sensors, sensors without a state yet and NaN states are all
// encoded as N2K "not available".
static double sensor_value_or_na(sensor::Sensor *sens) {
  if (sens == nullptr || !sens->has_state() || std::isnan(sens->state))
    return N2kDoubleNA;
  return sens->state;
}

void Nmea2000Component::setup() {
  this->n2k_ = new Nmea2000Twai(this->tx_pin_, this->rx_pin_);  // NOLINT

  // Reuse the source address claimed in a previous session so the node keeps
  // its identity on the bus across reboots (ISO address claim requirement).
  this->source_pref_ = global_preferences->make_preference<uint8_t>(fnv1_hash("nmea2000_source"));
  uint8_t source = this->source_address_;
  uint8_t stored_source;
  if (this->source_pref_.load(&stored_source) && stored_source <= 251) {
    source = stored_source;
    ESP_LOGD(TAG, "Restored previously claimed source address %u", source);
  }

  char serial[13];
  std::snprintf(serial, sizeof(serial), "%08u", static_cast<unsigned>(this->unique_number_));
  this->n2k_->SetProductInformation(serial, this->product_code_, this->product_name_.c_str(),
                                    this->software_version_.c_str());
  this->n2k_->SetDeviceInformation(this->unique_number_, this->device_function_, this->device_class_,
                                   this->manufacturer_code_);
  this->n2k_->SetMode(tNMEA2000::N2km_ListenAndNode, source);
  this->n2k_->EnableForward(false);
  g_component = this;
  this->n2k_->SetMsgHandler([](const tN2kMsg &msg) {
    if (g_component != nullptr)
      g_component->handle_message_(msg);
  });
  this->n2k_->SetN2kCANMsgBufSize(8);
  this->n2k_->SetN2kCANSendFrameBufSize(150);

  if (!this->n2k_->Open()) {
    ESP_LOGE(TAG, "Failed to open TWAI driver (tx=GPIO%u rx=GPIO%u)", this->tx_pin_, this->rx_pin_);
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "NMEA2000 node started, claiming source address %u", source);
}

void Nmea2000Component::loop() {
  this->n2k_->ParseMessages();

  if (this->n2k_->ReadResetAddressChanged()) {
    uint8_t address = this->n2k_->GetN2kSource();
    this->source_pref_.save(&address);
    global_preferences->sync();
    ESP_LOGW(TAG, "Source address changed to %u (persisted)", address);
  }

  this->process_transmit_entries_();
  this->check_bus_status_();
}

void Nmea2000Component::add_battery_status(uint32_t interval_ms, uint8_t instance, sensor::Sensor *voltage,
                                           sensor::Sensor *current, sensor::Sensor *temperature) {
  this->transmit_entries_.push_back(
      {PgnKind::BATTERY_STATUS, interval_ms, instance, 0, voltage, current, temperature});
}

void Nmea2000Component::add_dc_detailed_status(uint32_t interval_ms, uint8_t instance, DcType dc_type,
                                               sensor::Sensor *state_of_charge, sensor::Sensor *time_remaining) {
  this->transmit_entries_.push_back({PgnKind::DC_DETAILED_STATUS, interval_ms, instance,
                                     static_cast<uint8_t>(dc_type), state_of_charge, time_remaining, nullptr});
}

void Nmea2000Component::add_temperature(uint32_t interval_ms, uint8_t instance, TempSource source,
                                        sensor::Sensor *actual) {
  this->transmit_entries_.push_back(
      {PgnKind::TEMPERATURE, interval_ms, instance, static_cast<uint8_t>(source), actual, nullptr, nullptr});
}

void Nmea2000Component::add_humidity(uint32_t interval_ms, uint8_t instance, HumiditySource source,
                                     sensor::Sensor *actual) {
  this->transmit_entries_.push_back(
      {PgnKind::HUMIDITY, interval_ms, instance, static_cast<uint8_t>(source), actual, nullptr, nullptr});
}

void Nmea2000Component::process_transmit_entries_() {
  const uint32_t now = millis();
  for (auto &entry : this->transmit_entries_) {
    if (now - entry.last_send < entry.interval_ms)
      continue;
    entry.last_send = now;
    this->send_entry_(entry);
    // At most one PGN per loop() pass to keep the loop budget small; due
    // entries fire on the immediately following passes (µs apart).
    break;
  }
}

void Nmea2000Component::send_entry_(TransmitEntry &entry) {
  tN2kMsg msg;
  switch (entry.kind) {
    case PgnKind::BATTERY_STATUS: {
      double temperature = sensor_value_or_na(entry.c);
      if (!N2kIsNA(temperature))
        temperature = CToKelvin(temperature);
      SetN2kDCBatStatus(msg, entry.instance, sensor_value_or_na(entry.a), sensor_value_or_na(entry.b), temperature,
                        0xff);
      break;
    }
    case PgnKind::DC_DETAILED_STATUS: {
      double soc = sensor_value_or_na(entry.a);
      unsigned char soc_pct = N2kUInt8NA;
      if (!N2kIsNA(soc))
        soc_pct = static_cast<unsigned char>(clamp(soc, 0.0, 100.0));
      double time_remaining = sensor_value_or_na(entry.b);
      if (!N2kIsNA(time_remaining))
        time_remaining *= 60.0;  // configured in minutes, PGN wants seconds
      SetN2kDCStatus(msg, 0xff, entry.instance, static_cast<tN2kDCType>(entry.enum_value), soc_pct, N2kUInt8NA,
                     time_remaining);
      break;
    }
    case PgnKind::TEMPERATURE: {
      double temperature = sensor_value_or_na(entry.a);
      if (!N2kIsNA(temperature))
        temperature = CToKelvin(temperature);
      SetN2kTemperature(msg, 0xff, entry.instance, static_cast<tN2kTempSource>(entry.enum_value), temperature);
      break;
    }
    case PgnKind::HUMIDITY: {
      SetN2kHumidity(msg, 0xff, entry.instance, static_cast<tN2kHumiditySource>(entry.enum_value),
                     sensor_value_or_na(entry.a));
      break;
    }
  }

  if (this->n2k_->SendMsg(msg)) {
    ESP_LOGV(TAG, "TX PGN %lu instance %u", msg.PGN, entry.instance);
  } else {
    ESP_LOGV(TAG, "TX PGN %lu instance %u failed (queued frames full?)", msg.PGN, entry.instance);
  }
}

void Nmea2000Component::publish_(SensorType type, float value) {
  for (auto *sens : this->sensors_) {
    if (sens->get_type() == type)
      sens->publish_state(value);
  }
}

void Nmea2000Component::publish_wind_(SensorType type, uint8_t reference, float value) {
  for (auto *sens : this->sensors_) {
    if (sens->get_type() == type && static_cast<uint8_t>(sens->get_wind_reference()) == reference)
      sens->publish_state(value);
  }
}

void Nmea2000Component::handle_message_(const tN2kMsg &msg) {
  if (this->sensors_.empty())
    return;

  switch (msg.PGN) {
    case 129025UL: {  // Position, Rapid Update
      double latitude, longitude;
      if (!ParseN2kPGN129025(msg, latitude, longitude))
        return;
      ESP_LOGV(TAG, "RX 129025 lat=%.6f lon=%.6f", latitude, longitude);
      if (!N2kIsNA(latitude))
        this->publish_(SensorType::LATITUDE, latitude);
      if (!N2kIsNA(longitude))
        this->publish_(SensorType::LONGITUDE, longitude);
      break;
    }
    case 129026UL: {  // COG & SOG, Rapid Update
      unsigned char sid;
      tN2kHeadingReference reference;
      double cog, sog;
      if (!ParseN2kPGN129026(msg, sid, reference, cog, sog))
        return;
      ESP_LOGV(TAG, "RX 129026 cog=%.3f sog=%.2f ref=%d", cog, sog, static_cast<int>(reference));
      if (!N2kIsNA(sog))
        this->publish_(SensorType::SPEED_OVER_GROUND, msToKnots(sog));
      // Only publish COG referenced to true north; magnetic COG would need
      // variation applied and is not comparable across sources.
      if (!N2kIsNA(cog) && reference == N2khr_true)
        this->publish_(SensorType::COURSE_OVER_GROUND, RadToDeg(cog));
      break;
    }
    case 127250UL: {  // Vessel Heading
      unsigned char sid;
      double heading, deviation, variation;
      tN2kHeadingReference reference;
      if (!ParseN2kPGN127250(msg, sid, heading, deviation, variation, reference))
        return;
      ESP_LOGV(TAG, "RX 127250 heading=%.3f ref=%d", heading, static_cast<int>(reference));
      if (!N2kIsNA(heading))
        this->publish_(SensorType::HEADING, RadToDeg(heading));
      break;
    }
    case 128267UL: {  // Water Depth
      unsigned char sid;
      double depth, offset, range;
      if (!ParseN2kPGN128267(msg, sid, depth, offset, range))
        return;
      ESP_LOGV(TAG, "RX 128267 depth=%.2f offset=%.2f", depth, offset);
      if (N2kIsNA(depth))
        return;
      // Positive offset = distance transducer to waterline: publish depth
      // below surface. Negative offset (below keel) or NA: raw transducer depth.
      if (!N2kIsNA(offset) && offset > 0.0)
        depth += offset;
      this->publish_(SensorType::WATER_DEPTH, depth);
      break;
    }
    case 128259UL: {  // Speed, Water Referenced
      unsigned char sid;
      double water_referenced, ground_referenced;
      tN2kSpeedWaterReferenceType swrt;
      if (!ParseN2kPGN128259(msg, sid, water_referenced, ground_referenced, swrt))
        return;
      ESP_LOGV(TAG, "RX 128259 stw=%.2f", water_referenced);
      if (!N2kIsNA(water_referenced))
        this->publish_(SensorType::SPEED_THROUGH_WATER, msToKnots(water_referenced));
      break;
    }
    case 130306UL: {  // Wind Data
      unsigned char sid;
      double wind_speed, wind_angle;
      tN2kWindReference reference;
      if (!ParseN2kPGN130306(msg, sid, wind_speed, wind_angle, reference))
        return;
      ESP_LOGV(TAG, "RX 130306 speed=%.2f angle=%.3f ref=%d", wind_speed, wind_angle, static_cast<int>(reference));
      if (reference > N2kWind_True_water)
        return;  // error / unavailable
      auto ref = static_cast<uint8_t>(reference);
      if (!N2kIsNA(wind_speed))
        this->publish_wind_(SensorType::WIND_SPEED, ref, msToKnots(wind_speed));
      if (!N2kIsNA(wind_angle))
        this->publish_wind_(SensorType::WIND_ANGLE, ref, RadToDeg(wind_angle));
      break;
    }
    default:
      break;
  }
}

void Nmea2000Component::check_bus_status_() {
  uint32_t now = millis();
  if (now - this->last_bus_check_ < BUS_CHECK_INTERVAL_MS)
    return;
  this->last_bus_check_ = now;

  twai_status_info_t status;
  if (!this->n2k_->get_status(&status))
    return;

  switch (status.state) {
    case TWAI_STATE_BUS_OFF:
      ESP_LOGW(TAG, "CAN bus-off (tx_err=%" PRIu32 " rx_err=%" PRIu32 "), initiating recovery",
               status.tx_error_counter, status.rx_error_counter);
      this->n2k_->initiate_recovery();
      break;
    case TWAI_STATE_STOPPED:
      ESP_LOGW(TAG, "CAN controller stopped, restarting");
      this->n2k_->start();
      break;
    case TWAI_STATE_RUNNING:
      if (status.tx_error_counter >= 128 || status.rx_error_counter >= 128) {
        if (!this->error_passive_logged_) {
          ESP_LOGW(TAG, "CAN error passive (tx_err=%" PRIu32 " rx_err=%" PRIu32 ")", status.tx_error_counter,
                   status.rx_error_counter);
          this->error_passive_logged_ = true;
        }
      } else {
        this->error_passive_logged_ = false;
      }
      break;
    default:
      break;
  }
}

void Nmea2000Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NMEA2000:");
  ESP_LOGCONFIG(TAG, "  CAN TX Pin: GPIO%u", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  CAN RX Pin: GPIO%u", this->rx_pin_);
  ESP_LOGCONFIG(TAG, "  Bitrate: 250 kbit/s (fixed)");
  ESP_LOGCONFIG(TAG, "  Node: source=%u unique=%u manufacturer=%u function=%u class=%u",
                this->n2k_ != nullptr ? this->n2k_->GetN2kSource() : this->source_address_, this->unique_number_,
                this->manufacturer_code_, this->device_function_, this->device_class_);
  ESP_LOGCONFIG(TAG, "  Product: '%s' code=%u sw='%s'", this->product_name_.c_str(), this->product_code_,
                this->software_version_.c_str());
  ESP_LOGCONFIG(TAG, "  Transmit entries: %u", static_cast<unsigned>(this->transmit_entries_.size()));
  ESP_LOGCONFIG(TAG, "  Registered sensors: %u", static_cast<unsigned>(this->sensors_.size()));
}

}  // namespace nmea2000
}  // namespace esphome
