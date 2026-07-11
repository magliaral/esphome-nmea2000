# esphome-nmea2000

ESPHome external component that turns an ESP32-S3 into a full NMEA2000 node —
transmit ESPHome sensor values onto the bus (shown on chartplotters like the
Raymarine Axiom) and receive navigation data from the backbone as regular
ESPHome sensors (Home Assistant, displays, automations, ...).

Built on the excellent [NMEA2000 library by Timo Lappalainen](https://github.com/ttlappalainen/NMEA2000)
with a lean built-in TWAI (CAN) driver for the ESP32-S3 family. There is
currently no NMEA2000 component in ESPHome core (see
[esphome/feature-requests#2760](https://github.com/esphome/feature-requests/issues/2760)).

## Features

- **Full NMEA2000 node**: ISO address claim, product/device information,
  answers to ISO requests — the device shows up properly in the device list of
  MFDs (tested target: Raymarine Axiom). The claimed source address is
  persisted in flash and reused after reboots.
- **Declarative transmit mapping**: map any ESPHome sensor onto N2K PGNs —
  battery status (127508), DC detailed status / SOC (127506, fast packet),
  temperature (130312 and 130316 extended range) and humidity (130313).
  Missing or unavailable values are encoded as N2K *not available*.
- **Sensor platform for navigation data**: position, SOG/COG, heading, water
  depth, speed through water, wind and water temperature (129025, 129026,
  127250, 128267, 128259, 130306, 130310/130312/130316) with user-friendly
  units (degrees, knots, meters, °C).
- **Robust bus handling**: fixed 250 kbit/s (NMEA2000 standard), bus-off
  recovery, error-passive warnings, non-blocking — the ESPHome loop is never
  stalled.

Tested on the LILYGO T-Connect-Pro (ESP32-S3-R8, onboard CAN transceiver on
TX GPIO6 / RX GPIO7), Arduino framework, ESPHome ≥ 2026.6.

## Wiring

Connect the node to the backbone as a regular **drop**: CAN_H, CAN_L and GND
via a short stub cable to a T-piece on the backbone.

- Do **not** add a termination resistor — the backbone is already terminated
  at both ends.
- The onboard transceiver of the T-Connect-Pro is **not galvanically
  isolated**; the board GND is tied to NMEA2000 GND (NET-C). Power the board
  either from the bus supply or make sure both supplies share ground.

## Usage with ESPHome

```yaml
external_components:
  - source: github://magliaral/esphome-nmea2000
    components: [nmea2000]

nmea2000:
  id: n2k
  can:
    tx_pin: GPIO6
    rx_pin: GPIO7

  node:
    source_address: 35         # preferred address, the lib re-claims if taken
    unique_number: 1
    product_name: "SY Alema Bridge"

  transmit:
    - pgn: battery_status              # 127508
      instance: 0
      voltage: shunt_battery_voltage
      current: shunt_battery_current
    - pgn: dc_detailed_status          # 127506 (fast packet)
      state_of_charge: shunt_battery_state_of_charge
      time_remaining: shunt_battery_time_remaining   # minutes
    - pgn: temperature_extended        # 130316 (use this for Raymarine MFDs)
      source: inside
      actual: salon_temperature
    - pgn: humidity                    # 130313
      actual: salon_humidity

sensor:
  - platform: nmea2000
    type: WATER_DEPTH
    name: "Water Depth"
  - platform: nmea2000
    type: WIND_SPEED
    wind_reference: apparent
    name: "Apparent Wind Speed"
```

A complete, standalone-compiling example for the LILYGO T-Connect-Pro is in
[examples/sy-alema.yaml](examples/sy-alema.yaml).

## Configuration variables

### `nmea2000:` (hub)

- **id** (*Optional*): Manually specify the ID used for code generation.
- **can** (**Required**):
  - **tx_pin** (**Required**): GPIO connected to the CAN transceiver TX input.
  - **rx_pin** (**Required**): GPIO connected to the CAN transceiver RX output.
  - The bitrate is fixed at 250 kbit/s as required by the NMEA2000 standard.
- **node** (*Optional*): NMEA2000 node identity.
  - **source_address** (*Optional*, default `15`): Preferred source address
    (0–251). The library claims a different one automatically if it is taken;
    the claimed address is persisted and reused after reboots.
  - **unique_number** (*Optional*, default `1`): Unique number for the ISO
    NAME (1–2097151). Use a different one for each of your devices.
  - **manufacturer_code** (*Optional*, default `2046`): 2046 is the de-facto
    open source / DIY code.
  - **device_function** (*Optional*, default `130`): 130 = PC Gateway (within
    device class 25).
  - **device_class** (*Optional*, default `25`): 25 = Inter/Intranetwork
    Device.
  - **product_name** (*Optional*, default `ESPHome NMEA2000`): Shown in the
    device list of MFDs (max. 32 characters).
  - **product_code** (*Optional*, default `100`).
  - **software_version** (*Optional*, default `1.0.0`).
- **transmit** (*Optional*): List of PGNs to send periodically. Sensors that
  have no state (yet) or are `NaN` are encoded as N2K *not available*.
  Configuring the same `pgn` twice with the same `instance` is rejected.

  Common options for every entry:
  - **pgn** (**Required**): One of `battery_status`, `dc_detailed_status`,
    `temperature`, `humidity`.
  - **interval** (*Optional*): Transmit interval. Defaults: `2s` for
    `battery_status`/`dc_detailed_status`, `5s` for `temperature`/`humidity`.
  - **instance** (*Optional*, default `0`): N2K instance (0–252), used by
    displays to distinguish multiple batteries/sensors.

  Per PGN:
  - `battery_status` (PGN 127508, single frame):
    - **voltage** (**Required**): Sensor ID, volts.
    - **current** (*Optional*): Sensor ID, amps.
    - **temperature** (*Optional*): Sensor ID, °C.
  - `dc_detailed_status` (PGN 127506, fast packet — handled by the library):
    - **dc_type** (*Optional*, default `battery`): One of `battery`,
      `alternator`, `converter`, `solar_cell`, `wind_generator`.
    - **state_of_charge** (*Optional*): Sensor ID, percent (0–100).
    - **time_remaining** (*Optional*): Sensor ID, **minutes**.
    - At least one of `state_of_charge` / `time_remaining` is required.
  - `temperature` (PGN 130312):
    - **source** (**Required**): One of `sea`, `outside`, `inside`,
      `engine_room`, `main_cabin`, `live_well`, `bait_well`, `refrigeration`,
      `heating_system`, `dew_point`, `apparent_wind_chill`,
      `theoretical_wind_chill`, `heat_index`, `freezer`, `exhaust_gas`,
      `shaft_seal`.
    - **actual** (**Required**): Sensor ID, °C.
  - `temperature_extended` (PGN 130316, Temperature Extended Range): same
    options as `temperature`. **Raymarine LightHouse (e.g. Axiom) ignores
    PGN 130312** and reliably shows cabin temperatures only via 130316 with
    source `inside` — use `temperature_extended` when the data should show
    up on a Raymarine MFD. `temperature` and `temperature_extended` are
    separate PGNs, so the same `instance` may be used in both.
  - `humidity` (PGN 130313):
    - **source** (*Optional*, default `inside`): `inside` or `outside`.
    - **actual** (**Required**): Sensor ID, percent.

### `sensor:` platform

- **nmea2000_id** (*Optional*): ID of the hub, only needed with multiple hubs.
- **type** (**Required**): What to receive from the bus:

  | type | PGN | unit | notes |
  |---|---|---|---|
  | `LATITUDE` | 129025 | ° | |
  | `LONGITUDE` | 129025 | ° | |
  | `SPEED_OVER_GROUND` | 129026 | kn | |
  | `COURSE_OVER_GROUND` | 129026 | ° | only published when referenced to true north |
  | `HEADING` | 127250 | ° | published for any reference (usually magnetic) |
  | `WATER_DEPTH` | 128267 | m | transducer offset applied when positive (depth below surface) |
  | `SPEED_THROUGH_WATER` | 128259 | kn | |
  | `WIND_SPEED` | 130306 | kn | filtered by `wind_reference` |
  | `WIND_ANGLE` | 130306 | ° | filtered by `wind_reference` |
  | `WATER_TEMPERATURE` | 130310, 130312, 130316 | °C | 130312/130316 only with source `sea`; first available source per frame wins |

- **wind_reference** (*Optional*, default `apparent`, wind types only): Which
  wind reference to listen to — `apparent`, `true_boat`, `true_water`,
  `true_north`, `magnetic`.
- All other options from [Sensor](https://esphome.io/components/sensor/)
  (`name`, `id`, `filters`, ...). Sensible defaults for
  `unit_of_measurement`, `icon` and `accuracy_decimals` are set per type and
  can be overridden.

N2K *not available* values are never published, so the sensors keep their
last valid state.

### A note on position precision

`LATITUDE`/`LONGITUDE` are published as regular ESPHome sensor states, which
are 32-bit floats — that quantizes positions to roughly 0.5–1 m. Fine for
dashboards and anchor-watch style displays; precise track logging would need
a combined `text_sensor` with full resolution (planned for a later version).

## Not (yet) supported

- Fast-packet reception (e.g. 129029 GNSS), `text_sensor`/`binary_sensor`
  platforms, raw `on_message` trigger — candidates for v2.
- NMEA0183, SignalK.

## Disclaimer

NMEA2000® is a registered trademark of the National Marine Electronics
Association. This project is neither certified nor endorsed by the NMEA. It
is a hobbyist implementation intended for private use — use it at your own
risk and never rely on it as your only source of navigation data.

## License

MIT — see [LICENSE](LICENSE). The
[NMEA2000 library](https://github.com/ttlappalainen/NMEA2000) pulled in as a
dependency is © Timo Lappalainen, MIT license.
