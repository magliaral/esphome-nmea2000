#include "nmea2000.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>

namespace esphome {
namespace nmea2000 {

static const char *const TAG = "nmea2000";

static const uint32_t BUS_CHECK_INTERVAL_MS = 2000;

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

  this->check_bus_status_();
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
}

}  // namespace nmea2000
}  // namespace esphome
