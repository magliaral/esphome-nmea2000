#pragma once

#include <NMEA2000.h>
#include <driver/twai.h>

namespace esphome {
namespace nmea2000 {

// Minimal tNMEA2000 driver for the ESP32 TWAI peripheral (ESP32, S2, S3, C3, ...).
// NMEA2000 is CAN 2.0B at a fixed 250 kbit/s with 29-bit extended identifiers.
class Nmea2000Twai : public tNMEA2000 {
 public:
  Nmea2000Twai(uint8_t tx_pin, uint8_t rx_pin) : tx_pin_(tx_pin), rx_pin_(rx_pin) {}

  bool get_status(twai_status_info_t *status);
  // Kick the controller out of bus-off; returns to STOPPED state when done.
  bool initiate_recovery();
  // (Re-)start a controller that is in STOPPED state.
  bool start();

 protected:
  bool CANOpen() override;
  bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent = true) override;
  bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) override;

  uint8_t tx_pin_;
  uint8_t rx_pin_;
  bool driver_installed_{false};
};

}  // namespace nmea2000
}  // namespace esphome
