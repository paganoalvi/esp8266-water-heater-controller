# ESP8266 Water Heater Controller

Control system for a household electric water heater using an ESP8266 microcontroller, Wi-Fi connectivity, NTP time synchronization, a relay, and a local web interface.

The project is being developed incrementally. The ESP8266 currently operates as a standalone controller connected to the local Wi-Fi network and hosts a web interface that can be accessed from computers and mobile phones on the same network.

The actual household water heater is **not yet connected to the controller**. Current tests are performed only with the controller hardware and relay.

---

## Current Status

The project currently has the following functionality:

* ESP8266 initialization.
* Wi-Fi connection.
* NTP time synchronization.
* Local time calculation using UTC-3.
* Automatic relay control according to a fixed schedule.
* Manual/automatic control mode architecture.
* Relay abstraction using `setRelay()`.
* Local HTTP web server running on the ESP8266.
* LittleFS filesystem.
* HTML web interface stored in LittleFS.
* CSS styling stored in LittleFS.
* Web interface accessible from computers and mobile phones connected to the same local Wi-Fi network.
* Serial output for monitoring system status.

The current web interface is a first version with a terminal/Matrix-inspired visual design.

The next development step is to connect the web interface controls to the ESP8266 control logic so that the relay can be controlled from a phone or computer.

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

The firmware defines:

```cpp
const uint8_t RELAY_PIN = 0;
```

The relay uses inverted logic:

```text
GPIO LOW  → Relay ON
GPIO HIGH → Relay OFF
```

The firmware hides this hardware-specific behavior behind the function:

```cpp
void setRelay(bool turnOn)
```

This allows the rest of the application to work with the logical concepts `ON` and `OFF` without depending on the physical GPIO polarity.

The relay has been tested independently during development.

The actual household water heater remains disconnected from the controller.

### Controller board

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

The firmware has been tested with:

```text
3.1.2
```

### Libraries

The current firmware uses:

* `ESP8266WiFi`
* `WiFiUdp`
* `NTPClient`
* `LittleFS`
* `ESP8266WebServer`

### Serial communication

Serial communication is configured at:

```text
9600 baud
```

The firmware periodically prints information similar to:

```text
Time: 22:55:12 | Relay: OFF | Mode: AUTO | Schedule: INACTIVE
```

The serial output is currently used for development, testing, and debugging.

---

## Wi-Fi

The ESP8266 connects to the local Wi-Fi network during startup.

The current firmware waits for a Wi-Fi connection before continuing with the rest of the initialization.

Wi-Fi credentials are stored locally in:

```text
secrets.h
```

This file is excluded from Git using `.gitignore` and must not be committed to the public repository.

The firmware includes:

```cpp
#include "secrets.h"
```

with the credentials supplied through local constants.

The public repository must never contain the actual Wi-Fi credentials.

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

The firmware uses the `NTPClient` library.

The current time is represented internally by:

* Hour
* Minute
* Second

The scheduling logic currently uses the number of minutes elapsed since midnight.

---

## Automatic Heating Schedule

The current automatic schedule is:

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

The current schedule is represented internally using schedule structures.

Example:

```cpp
struct Schedule
{
  uint16_t startMinutes;
  uint16_t endMinutes;
};
```

The current schedules are still defined in the firmware and are not yet configurable through the web interface.

---

## Control Modes

The controller has two logical operating modes:

```text
AUTO
MANUAL
```

These modes are represented by:

```cpp
enum ControlMode
{
  MODE_AUTO,
  MODE_MANUAL
};
```

### AUTO mode

In automatic mode, the relay state is determined exclusively by the configured heating schedule.

Conceptually:

```text
Current time
     ↓
Schedule evaluation
     ↓
Relay ON / OFF
```

### MANUAL mode

In manual mode, the relay is controlled explicitly through the manual relay state.

Conceptually:

```text
Manual command
     ↓
Manual relay state
     ↓
Relay ON / OFF
```

The logical manual state is stored separately from the physical relay state.

This allows the controller to distinguish between:

* The desired manual state.
* The actual relay state.

### Web interface behavior

The web interface will include a visible indicator showing whether the controller is currently in:

```text
AUTOMATIC
```

or:

```text
MANUAL
```

When the controller is in `AUTO` mode, the manual `ENCENDER` and `APAGAR` controls should not be available for direct operation.

The interface may either:

* Disable the manual controls, or
* Allow the user to press them but display a message explaining that the controller must first be changed to `MANUAL`.

The final UI behavior will be implemented together with the control API.

The important design rule is:

> Manual relay commands must not silently override automatic scheduling while the controller is in `AUTO` mode.

---

## Relay Control Abstraction

The physical relay uses inverted GPIO logic.

Instead of manipulating the GPIO directly throughout the application, the firmware uses:

```cpp
void setRelay(bool turnOn)
```

Conceptually:

```text
Logical state
    ON / OFF
       ↓
   setRelay()
       ↓
GPIO LOW / HIGH
       ↓
Physical relay
```

This keeps hardware-specific details isolated from the scheduling and web-control logic.

---

## Web Server

The ESP8266 currently runs a local HTTP server using:

```cpp
ESP8266WebServer server(80);
```

The web server is accessible through the ESP8266's local IP address.

For example:

```text
http://192.168.x.x/
```

The ESP8266 and the client device must be connected to the same local Wi-Fi network.

The web server currently serves the main interface from LittleFS.

---

## LittleFS

The project uses LittleFS to store web files in the ESP8266 flash memory.

Current web files:

```text
data/
├── index.html
└── style.css
```

The LittleFS image is uploaded separately from the firmware.

The HTML references the stylesheet using a relative path:

```html
<link rel="stylesheet" href="./style.css">
```

This allows the same `index.html` and `style.css` structure to be opened locally during development and served by the ESP8266.

The current interface has been successfully tested from both:

* A computer connected to the local Wi-Fi.
* A mobile phone connected to the same Wi-Fi.

---

## Web Interface

The current interface is a first visual version.

The design uses a black background and green terminal/Matrix-inspired styling.

The interface currently displays concepts such as:

* Connection status.
* Current heater state.
* Operating mode.
* Current time.
* Heating schedule.
* Manual control buttons.
* System information.

Some values displayed by the current HTML are still static placeholders.

The next development stage is to replace these placeholders with real data obtained from the ESP8266.

---

## Planned Web API

The web interface will communicate with the ESP8266 through HTTP endpoints.

The planned architecture includes endpoints such as:

```text
GET  /api/status
GET  /api/config
POST /api/config
POST /api/control
```

The first endpoint to be implemented will be the relay control endpoint:

```text
POST /api/control
```

The intended flow is:

```text
Phone / Computer
       ↓
    Web UI
       ↓
 JavaScript request
       ↓
POST /api/control
       ↓
ESP8266
       ↓
Control logic
       ↓
setRelay()
       ↓
Relay
```

The API will respect the current operating mode.

For example, a manual relay command must not bypass the automatic schedule while the controller is in `AUTO` mode.

---

## System Architecture

The current conceptual architecture is:

```text
                 ┌──────────────────────┐
                 │      Web Browser     │
                 │  PC / Mobile Phone  │
                 └──────────┬───────────┘
                            │
                         Wi-Fi
                            │
                 ┌──────────▼───────────┐
                 │       ESP8266        │
                 │                      │
                 │  HTTP Web Server     │
                 │  Control Logic       │
                 │  Schedule Logic      │
                 │  NTP Client          │
                 └──────────┬───────────┘
                            │
                       setRelay()
                            │
                 ┌──────────▼───────────┐
                 │        Relay         │
                 └──────────────────────┘
```

The control logic is intentionally kept inside the ESP8266.

The web interface acts as a client of the controller rather than becoming the controller itself.

This means that the ESP8266 should remain capable of controlling the heater according to its configured schedule even if the web interface is not being accessed.

---

## Current Firmware Behavior

At startup, the ESP8266:

1. Initializes the serial port.
2. Configures the relay GPIO as an output.
3. Sets the relay to a safe initial state.
4. Connects to the configured Wi-Fi network.
5. Waits for the Wi-Fi connection.
6. Starts the NTP client.
7. Mounts LittleFS.
8. Starts the local HTTP server.

During the main loop, the firmware currently:

1. Handles HTTP client requests.
2. Updates the NTP client.
3. Updates the current time.
4. Evaluates the control mode.
5. Evaluates the automatic schedule when required.
6. Updates the relay state.
7. Prints system status through the serial port.

The current implementation still uses `delay()` and will eventually need to become more non-blocking.

---

## Known Limitations

### Fixed schedule

The current heating schedule is hard-coded in the firmware.

Changing the schedule currently requires modifying and uploading the firmware.

### Web controls are still under development

The web interface currently displays manual control buttons, but the HTTP API and JavaScript control logic are still being developed.

### Manual/automatic mode UI

The control mode architecture exists in the firmware, but the web interface still needs to provide complete mode selection and enforcement.

### Wi-Fi failure handling

The firmware currently waits for a Wi-Fi connection during startup.

There is no complete reconnection or timeout strategy yet.

### NTP failure handling

There is currently no complete strategy for loss of NTP synchronization.

### Blocking delay

The main loop currently uses `delay()`.

This will eventually need to be replaced or reduced so that the ESP8266 can perform other tasks without unnecessary blocking.

### Persistent configuration

The schedule and other configuration values are not yet stored persistently.

### Temperature measurement

The current interface may display a temperature placeholder, but there is currently no temperature sensor integrated into the controller.

### Remote access

The current web interface is only intended for devices connected to the same local Wi-Fi network.

No Internet-facing access has been implemented.

### High-voltage appliance

The actual household water heater is still disconnected from the controller.

All current tests are performed using the controller hardware only.

---

## Development Plan

The project is being developed incrementally.

### A — Documentation

Document the hardware, firmware behavior, architecture, and development process.

This README represents the current project baseline.

### B — Code Refactoring

Continue improving the firmware structure while preserving its behavior.

Goals include:

* Clear naming.
* Separation of responsibilities.
* Simple and reusable data structures.
* Separation between scheduling and hardware control.
* Centralized relay control.
* Organized test/debug functionality.
* Reduced blocking behavior.

### C — Control Logic Testing

The scheduling logic should eventually be testable independently from the ESP8266.

Important boundary cases include:

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

Additional tests will be required for:

* AUTO mode.
* MANUAL mode.
* Switching between modes.
* Manual commands while in AUTO mode.
* Manual commands while in MANUAL mode.

### D — Web Control

Implement the local HTTP API and connect the web interface to the controller.

Initial functionality:

* Display real relay state.
* Display real operating mode.
* Display current time.
* Enable manual relay control in MANUAL mode.
* Prevent or reject manual relay commands in AUTO mode.
* Provide a clear mode indicator.

### E — Configuration

Allow the schedule and operating mode to be configured through the web interface.

### F — Persistent Configuration

Store configuration in the ESP8266 so that it survives a restart.

### G — Firmware Robustness

Improve:

* Wi-Fi reconnection.
* NTP synchronization recovery.
* Safe startup behavior.
* Non-blocking timing.
* Error reporting.
* Separation between application logic and hardware control.

---

## Future Features

Once the current controller is stable, the project may be extended with:

### Configurable schedule

Allow heating periods to be changed without modifying the firmware.

### Persistent configuration

Store schedule and controller settings in non-volatile memory.

### Temperature sensor

Add real temperature measurement to the controller.

### Improved web interface

Display real-time:

* Current temperature.
* Relay state.
* Operating mode.
* Current time.
* Active schedule.
* Next scheduled event.

### Additional sensors

Future versions may include other sensors or monitoring features.

### Remote architecture

A future version may include an external server or backend.

The ESP8266 should remain capable of operating locally even if an external server is unavailable.

These features are not part of the current implementation.

---

## Development Workflow

The repository uses Git for version control.

The `main` branch represents a stable baseline.

The `develop` branch is used for ongoing development and testing.

The preferred development workflow is:

```text
Edit
  ↓
Compile
  ↓
Upload firmware / LittleFS
  ↓
Test on ESP8266
  ↓
Verify behavior
  ↓
Commit
  ↓
Push to develop
```

Changes should be tested before being committed.

Stable development milestones can later be merged into `main`.

### Firmware upload

Firmware is uploaded through the Arduino IDE while the controller is placed in programming mode.

After uploading:

1. Return the controller to execution mode.
2. Press RESET if necessary.
3. Verify the firmware through the serial monitor or web server.

### LittleFS upload

The LittleFS filesystem is uploaded separately from the firmware.

The serial monitor should be closed during the filesystem upload.

The controller must be placed in the appropriate programming/bootloader state so that the uploader can communicate with the ESP8266.

After the upload:

1. Return the controller to execution mode.
2. Reset the ESP8266.
3. Verify that the firmware starts normally.
4. Verify that the web interface is available.

---

## Project Structure

Current project structure:

```text
esp8266-water-heater-controller/
│
├── README.md
│
└── esp8266_water_heater/
    │
    ├── esp8266_water_heater.ino
    ├── secrets.h
    │
    └── data/
        ├── index.html
        └── style.css
```

`secrets.h` is a local file and must not be committed to the public repository.

The `data/` directory contains files that are uploaded to the ESP8266 LittleFS filesystem.

---

## Safety

The controller is intended to eventually control a household electric water heater.

The actual high-voltage electrical installation and safety mechanisms are outside the scope of the software.

During development, the water heater remains disconnected from the controller while the firmware, relay, network communication, and control logic are being tested.

Hardware modifications, electrical protection, relay ratings, isolation, grounding, enclosure, and all other electrical safety requirements must be independently verified before connecting the controller to the appliance.

---

## License

License to be defined.
