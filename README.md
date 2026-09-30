# ESP8266 Water Heater Controller

Control system for a household electric water heater using an ESP8266 microcontroller, Wi-Fi connectivity, NTP time synchronization, and a relay.

The current firmware implements a fixed daily heating schedule based on the time obtained from an NTP server.

## Current Status

The original firmware has been tested successfully on the ESP8266.

Current functionality:

* ESP8266 initialization.
* Wi-Fi connection.
* NTP time synchronization.
* Local time calculation using UTC-3.
* Relay control according to a fixed schedule.
* Serial output for monitoring the current time.

The water heater itself is **not yet connected to the controller**. Current tests are performed only with the controller hardware.

---

## Hardware

### Microcontroller

* ESP8266
* Detected as: `ESP8266EX`
* Crystal: 26 MHz
* Flash: 1 MB

### Relay

The relay control signal is connected to:

```text
GPIO 0
```

The firmware currently defines the relay pin as:

```cpp
#define relay 0
```

The exact relationship between the GPIO state (`HIGH` / `LOW`) and the physical relay state still needs to be verified on the actual hardware.

### Board

The controller board includes:

* ESP8266
* Relay/control circuitry
* USB/serial programming interface
* Programming/execution switch
* Reset button
* Power supply circuitry

The exact board/model information will be documented when available.

---

## Software

### Development environment

* Arduino IDE
* Linux Mint

### ESP8266 Arduino Core

The firmware has been tested with ESP8266 Arduino Core:

```text
3.1.2
```

### Libraries

The current firmware uses:

* `ESP8266WiFi`
* `WiFiUdp`
* `NTPClient`

### Serial communication

Serial communication is configured at:

```text
9600 baud
```

The current firmware periodically prints the synchronized time:

```text
23:30
23:31
23:32
```

The delay between readings is currently used for testing and may change during development.

---

## Wi-Fi

The ESP8266 connects to the local Wi-Fi network during startup.

The current firmware waits until a Wi-Fi connection is established before continuing with NTP initialization.

Wi-Fi credentials should **not** be stored in the public repository.

For development, credentials should eventually be moved to a local/private configuration mechanism.

---

## Time Synchronization

The controller obtains the current time from:

```text
pool.ntp.org
```

The configured UTC offset is:

```text
UTC-3
```

This corresponds to the local time used during the current development setup.

The firmware uses the `NTPClient` library to obtain:

* Hour
* Minute
* Second
* Day of week

Only the hour and minute are currently used by the heating logic.

---

## Current Heating Schedule

The current requirement is to operate the water heater according to the following schedule:

| Time          | Heater |
| ------------- | ------ |
| 00:00 – 05:59 | OFF    |
| 06:00 – 06:59 | ON     |
| 07:00 – 17:59 | OFF    |
| 18:00 – 18:59 | ON     |
| 19:00 – 23:59 | OFF    |

In simplified form:

```text
06:00 ───── 07:00
   ON

07:00 ───────────────── 18:00
              OFF

18:00 ───── 19:00
   ON

19:00 ───────────────── 06:00
              OFF
```

This schedule is currently hard-coded in the firmware.

It is expected to become configurable in a later version.

---

## Current Firmware Behavior

At startup, the ESP8266:

1. Initializes the serial port.
2. Configures the relay GPIO as an output.
3. Sets the initial relay GPIO state.
4. Connects to the configured Wi-Fi network.
5. Waits for the Wi-Fi connection.
6. Starts the NTP client.

During the main loop:

1. The NTP client is updated.
2. The current hour and minute are read.
3. The current time is printed through the serial port.
4. The relay control logic is evaluated.
5. The loop waits for the next iteration.

The current implementation uses `delay()` between iterations.

---

## Known Limitations

The current firmware is an initial working version and has several limitations.

### Fixed schedule

The heating schedule is hard-coded.

Changing the schedule currently requires modifying and uploading the firmware.

### Relay state definition

The actual meaning of:

```cpp
digitalWrite(relay, HIGH);
```

and:

```cpp
digitalWrite(relay, LOW);
```

with respect to the physical relay state still needs to be verified on the controller board.

### Wi-Fi failure handling

The firmware currently waits indefinitely for a Wi-Fi connection during startup.

There is no recovery strategy or timeout yet.

### NTP failure handling

There is currently no explicit handling for loss of NTP synchronization.

### Blocking delay

The main loop currently uses `delay()`.

This will eventually need to be replaced or reduced so that the ESP8266 can perform other tasks without blocking.

### Configuration

There is currently no persistent configuration system.

### Web interface

There is currently no web interface.

---

## Development Plan

The project will be developed incrementally.

### A — Documentation

Document the original working firmware and hardware behavior.

This README represents the current baseline.

### B — Code Refactoring

Clean up and reorganize the existing firmware while preserving its behavior.

The goal is to:

* Remove unnecessary code.
* Improve naming.
* Separate responsibilities.
* Improve comments.
* Make the scheduling logic easier to understand.
* Keep useful test/debug functionality organized.

### C — Logic Testing

Move the scheduling logic into code that can also be tested on a normal computer.

The goal is to test the schedule without requiring the ESP8266.

Important boundary cases will include:

* 05:59 → OFF
* 06:00 → ON
* 06:59 → ON
* 07:00 → OFF
* 17:59 → OFF
* 18:00 → ON
* 18:59 → ON
* 19:00 → OFF
* 23:30 → OFF
* 00:30 → OFF

### D — Firmware Robustness

Improve the firmware behavior for real-world operation.

Potential improvements include:

* Wi-Fi reconnection.
* NTP synchronization failure handling.
* Safe startup relay state.
* Non-blocking timing.
* Better error reporting.
* Improved separation between scheduling logic and hardware control.

---

## Future Features

Once the basic firmware is stable, the project may be extended with:

### Configurable schedule

Allow the heating periods to be changed without modifying the firmware.

### Web interface

The ESP8266 could host a small web interface accessible from a phone or computer connected to the same Wi-Fi network.

The interface could eventually display:

* Current time.
* Current heater/relay state.
* Morning heating period.
* Evening heating period.
* Automatic/manual mode.
* Configuration controls.

### Persistent configuration

Store the configured schedule so that it survives an ESP8266 restart.

### Additional sensors

Future versions may include temperature measurement or other sensors.

These features are not part of the current implementation.

---

## Development Workflow

The repository uses Git for version control.

The `main` branch represents a known working baseline.

Development changes should be made separately and merged into `main` only after testing.

Example workflow:

```bash
git checkout -b refactor
```

Make and test changes on the development branch.

After the changes are considered stable:

```bash
git checkout main
git merge refactor
```

---

## Project Structure

Current project structure:

```text
esp8266-termotanque/
├── esp8266_termotanque.ino
└── README.md
```

The structure will evolve as the project grows.

---

## Safety

The controller is intended to eventually control a household electric water heater.

The actual high-voltage electrical installation and safety mechanisms are outside the scope of the software.

During development, the water heater remains disconnected from the controller while the firmware and control logic are being tested.

Hardware modifications and electrical safety should be verified independently before connecting the controller to the appliance.

---

## License

License to be defined.

