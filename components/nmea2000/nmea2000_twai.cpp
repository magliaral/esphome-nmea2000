#include "nmea2000_twai.h"

#include "esphome/core/log.h"

#include <cstring>

namespace esphome {
namespace nmea2000 {

static const char *const TAG = "nmea2000";

bool Nmea2000Twai::CANOpen() {
  twai_general_config_t general =
      TWAI_GENERAL_CONFIG_DEFAULT(static_cast<gpio_num_t>(this->tx_pin_), static_cast<gpio_num_t>(this->rx_pin_),
                                  TWAI_MODE_NORMAL);
  general.tx_queue_len = 32;
  general.rx_queue_len = 32;
  twai_timing_config_t timing = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t err = twai_driver_install(&general, &timing, &filter);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
    return false;
  }
  this->driver_installed_ = true;

  err = twai_start();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "twai_start failed: %s", esp_err_to_name(err));
    twai_driver_uninstall();
    this->driver_installed_ = false;
    return false;
  }
  return true;
}

bool Nmea2000Twai::CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent) {
  twai_message_t message = {};
  message.identifier = id;
  message.extd = 1;
  message.data_length_code = len > 8 ? 8 : len;
  std::memcpy(message.data, buf, message.data_length_code);
  // Never block the ESPHome loop; on a full TX queue the NMEA2000 library
  // buffers the frame itself and retries from SendMsg()/ParseMessages().
  return twai_transmit(&message, 0) == ESP_OK;
}

bool Nmea2000Twai::CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) {
  twai_message_t message;
  if (twai_receive(&message, 0) != ESP_OK)
    return false;
  if (!message.extd || message.rtr)
    return false;  // NMEA2000 uses extended data frames only
  id = message.identifier;
  len = message.data_length_code > 8 ? 8 : message.data_length_code;
  std::memcpy(buf, message.data, len);
  return true;
}

bool Nmea2000Twai::get_status(twai_status_info_t *status) {
  if (!this->driver_installed_)
    return false;
  return twai_get_status_info(status) == ESP_OK;
}

bool Nmea2000Twai::initiate_recovery() { return twai_initiate_recovery() == ESP_OK; }

bool Nmea2000Twai::start() { return twai_start() == ESP_OK; }

}  // namespace nmea2000
}  // namespace esphome
