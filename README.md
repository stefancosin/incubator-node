# incubator-node

ESP-IDF/FreeRTOS firmware for an ESP32-based smart incubator: reads temperature/humidity, drives a heater relay, cooler MOSFET, alarm buzzer, and a 28BYJ-48 stepper motor for egg-turning, and talks to a Raspberry Pi over MQTT for telemetry and remote control.

Part of a larger system - see [`rpi-platform`](https://github.com/stefancosin/rpi-platform) for the RPi-side MQTT broker bridge and web dashboard this firmware talks to.

## Architecture

Three FreeRTOS tasks, each owning a disjoint set of hardware, communicating through queues instead of shared globals:

```
SensorTask ──┐
             ├──controlQueue──► ControlTask ──motorQueue──► MotorTask
MqttLink ────┘                  (LCD, heater,                (stepper)
                                  cooler, alarm)
```

- **SensorTask** - reads the DHT11 every 2s, publishes `incubator/sensors/temperature|humidity` over MQTT, and forwards a `SensorReading` event to `ControlTask`. Pinned to CPU core 1, away from the WiFi driver on core 0, after a standalone test showed the DHT11 read failing intermittently with WiFi active but succeeding 29/29 times in isolation.
- **ControlTask** - the single consumer of `controlQueue`, so LCD/relay writes from a sensor reading, an MQTT command, or a mode switch are never interleaved. Owns the LCD, heater relay, cooler MOSFET, and alarm buzzer directly. Runs the AUTO/MANUAL state machine and a software timer that triggers the periodic AUTO egg-turn.
- **MotorTask** - exclusively owns the stepper motor, consuming its own `motorQueue` so a 2048-step 180° turn (~2 seconds, one step per ms) never blocks sensor reads or MQTT handling. A MANUAL `off` command interrupts an in-progress AUTO turn immediately rather than waiting it out.
- **IncubatorState** - mutex-protected shared state (mode, last sensor reading, each actuator's on/off state, AUTO thresholds) that every task reads/writes through instead of plain globals. `ControlTask` and `MotorTask` are the only writers, each owning a disjoint set of peripherals.
- **MqttLink** - subscribes to `incubator/peripherals/+/set` and translates every recognized message into a `ControlEvent` pushed onto `ControlTask`'s queue. This is the only thing the MQTT event handler does - `ControlTask` remains the sole place actuator/mode decisions are actually made.

## Hardware

| Peripheral | Pin(s) | Notes |
|---|---|---|
| DHT11 (temp/humidity) | GPIO4 | Bit-banged, critical-section-protected read |
| LCD (I2C, PCF8574) | SDA=GPIO21, SCL=GPIO22 | 16x2, HD44780 |
| Alarm/buzzer | GPIO18 | |
| Heater relay | GPIO19 | |
| Cooler | GPIO23 | Driven by an IRFZ44N MOSFET, not a relay |
| Stepper (28BYJ-48) | IN1-4 = GPIO25/26/27/14 | Via a ULN2003 driver board, powered separately from the ESP32 to avoid brownout |

Adjust `main/incubator_config.h` if your board doesn't expose the same pins (several of the above were moved from more "obvious" choices because this specific board - likely a WROVER variant - doesn't expose GPIO16/17).

## MQTT

Connects to the broker configured via `idf.py menuconfig` and talks the topic scheme the RPi platform expects - see [`rpi-platform`'s README](https://github.com/stefancosin/rpi-platform#mqtt-topic-scheme) for the full table. In short:

- Publishes sensor readings to `incubator/sensors/<name>`
- Publishes each actuator's (and `mode`'s, and the AUTO thresholds') state to `incubator/peripherals/<name>/state` on every real transition, plus unconditionally once at boot so the dashboard is never stale from a previous session
- Subscribes to `incubator/peripherals/+/set` for incoming commands

Actuator `/set` commands are only honored in MANUAL mode; a `mode`/threshold `/set` is always honored.

## Building and flashing

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) (built against v5.3).

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32
idf.py menuconfig   # set WiFi SSID/password and the MQTT broker URI under
                     # "Incubator Node Configuration"
idf.py -p /dev/ttyUSB0 build flash monitor
```

`sdkconfig` (the generated file with your actual WiFi credentials in plaintext) is gitignored - only `sdkconfig.defaults` (placeholder values) is tracked.

Note: `idf.py monitor` resets the board on open (RTS/DTR toggle), which also wipes any runtime state set over MQTT (mode, AUTO thresholds) since `IncubatorState` is in-memory only.
