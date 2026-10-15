# ESP8266 Water Heater Controller

A DIY controller for a household electric water heater using an ESP8266, Wi-Fi, NTP time synchronization, a relay, and a browser-based web interface.

The project is being developed incrementally. The ESP8266 handles the time-based control logic locally. A separate web interface lets a user inspect the controller and change its configuration from a computer or phone connected to the home network.

> **Safety status:** The actual household water heater is not connected to the controller. Tests are being performed with the controller hardware and relay only.

---

## Current Status

The project currently includes:

- ESP8266 initialization and Wi-Fi connectivity.
- NTP time synchronization and local time using UTC-3.
- Automatic relay control based on configurable time ranges.
- `AUTO` and `MANUAL` operating modes.
- A relay abstraction through `setRelay()`.
- An HTTP API hosted by the ESP8266.
- A separate HTML/CSS/JavaScript web interface.
- A dark, green terminal-inspired interface for desktop and mobile.
- API-based display of controller status and controls for mode, schedules, and manual operation.
- Persistent storage of schedule and operating-mode configuration across ESP8266 restarts.
- Serial output for monitoring and debugging.

The interface is served by a separate Ubuntu Server laptop on the local network. It is **not** served by the ESP8266 and is **not** stored in the ESP8266's LittleFS filesystem in the current architecture.

The ESP8266 retains the local control logic. The web interface is a client and is not required for the controller to evaluate its schedule.

---

## Hardware

### Microcontroller

- ESP8266
- Detected chip: `ESP8266EX`
- Crystal: 26 MHz
- Flash: 1 MB

### Relay

The relay control signal is connected to GPIO 0.

```cpp
const uint8_t RELAY_PIN = 0;
```

The relay uses inverted logic:

```text
GPIO LOW  → Relay ON
GPIO HIGH → Relay OFF
```

The firmware hides this hardware-specific behavior behind:

```cpp
void setRelay(bool turnOn)
```

This lets the rest of the application use the logical states `ON` and `OFF` without depending on the physical GPIO polarity.

The relay has been tested independently during development. The household water heater remains disconnected.

### Controller board

The controller board includes an ESP8266, relay/control circuitry, a USB/serial programming interface, a programming/execution switch, a reset button, and power-supply circuitry. Exact board details can be added when available.

---

## Software and Development Environment

### Development environment

- Arduino IDE
- Linux Mint on the development computer
- Ubuntu Server 24.04 LTS on the separate web-interface server

### ESP8266 Arduino Core

The firmware has been tested with ESP8266 Arduino Core version:

```text
3.1.2
```

### Firmware libraries

The firmware uses:

- `ESP8266WiFi`
- `WiFiUdp`
- `NTPClient`
- `ESP8266WebServer`

The web interface is a separate set of static files:

- `index.html`
- `style.css`
- `js/api.js`

The interface server currently uses Python's built-in HTTP server. The planned Go backend/API is a separate next step and is not yet part of the deployed water-heater architecture.

### Serial communication

Serial communication is configured at 9600 baud. Serial output is used for development, testing, and debugging; the exact status line depends on the current firmware version.

---

## Wi-Fi and Credentials

The ESP8266 connects to the local Wi-Fi network during startup. Wi-Fi credentials are stored locally in:

```text
secrets.h
```

The firmware includes this file:

```cpp
#include "secrets.h"
```

`secrets.h` is excluded from Git using `.gitignore` and must never be committed to the public repository. The public repository must not contain actual Wi-Fi credentials.

The ESP8266 and the server laptop need to be connected to the same local network for the current setup to work.

---

## Time Synchronization

The controller obtains time from:

```text
pool.ntp.org
```

The configured UTC offset is UTC-3 (`-10800` seconds). The firmware uses the `NTPClient` library.

The scheduling logic represents time as the number of minutes elapsed since midnight. The web interface uses the controller's reported time and schedule data to display the next scheduled start when time synchronization is valid.

---

## Automatic Heating Schedule

The controller supports schedule entries with a start and end time. The initial example schedule has two daily windows:

- 06:00–07:00
- 18:00–19:00

The configured schedule can be changed through the web interface and is stored persistently by the ESP8266. The actual relay state in `AUTO` mode is determined by the configured schedule, not by whether the web interface is open.

A schedule entry is represented in firmware using a structure like:

```cpp
struct Schedule
{
  uint16_t startMinutes;
  uint16_t endMinutes;
};
```

Times are stored as minutes since midnight. The current implementation is intended for daily time ranges; behavior for ranges that cross midnight should be verified before relying on them.

---

## Control Modes

The controller has two operating modes:

```text
AUTO
MANUAL
```

### AUTO mode

In automatic mode, the relay state is determined by the configured heating schedule:

```text
Current time
    ↓
Schedule evaluation
    ↓
Relay ON / OFF
```

### MANUAL mode

In manual mode, the relay can be controlled explicitly through the manual controls:

```text
Manual command
    ↓
Manual relay state
    ↓
Relay ON / OFF
```

The firmware keeps the manual state conceptually separate from the physical relay state. Manual relay commands must not silently override automatic scheduling while the controller is in `AUTO` mode.

The web interface provides a mode selector and manual controls. The API and firmware are responsible for enforcing the selected mode.

---

## Relay Control Abstraction

The physical relay uses inverted GPIO logic. Instead of manipulating the GPIO directly throughout the application, the firmware uses:

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

This keeps hardware-specific details isolated from scheduling and web-control logic.

---

## System Architecture

The current setup has three main parts:

```text
┌─────────────────────────────┐
│ Browser on PC or phone      │
│ Web interface (HTML/CSS/JS) │
└──────────────┬──────────────┘
               │ HTTP
               ▼
┌─────────────────────────────┐
│ Ubuntu Server laptop        │
│ Static UI server, port 8081 │
└─────────────────────────────┘

Browser JavaScript sends API requests directly to the ESP8266
over the local network:

┌─────────────────────────────┐
│ ESP8266                     │
│ HTTP API                    │
│ Time and schedule logic     │
│ AUTO / MANUAL control       │
└──────────────┬──────────────┘
               │
               ▼
        ┌────────────┐
        │ Relay      │
        └────────────┘
```

The browser loads the interface from the Ubuntu Server laptop. The JavaScript in `UI/js/api.js` sends requests to the ESP8266's local IP address. In the current version, the browser communicates with the ESP8266 API directly; a central Go backend has not yet been integrated.

The ESP8266 should remain capable of controlling the relay according to its configured schedule even if the interface server is unavailable. Remote access over the public Internet has not been implemented, and the ESP8266 should not be exposed directly to the Internet.

---

## ESP8266 HTTP API

The ESP8266 runs a local HTTP server using `ESP8266WebServer`. Its API is available at the ESP8266's local IP address, for example:

```text
http://192.168.1.39/api/status
```

The exact address depends on the local network configuration and may change if the router assigns a different DHCP address. A DHCP reservation can be configured in the router to make the address predictable.

The current API includes endpoints for status, manual control, operating mode, and schedules. The interface uses these endpoints to retrieve state and send changes. Endpoint details should be kept aligned with the actual firmware and `UI/js/api.js`.

Examples of API paths include:

```text
GET  /api/status
POST /api/control
POST /api/mode
POST /api/schedules
```

The ESP8266 and the browser device must be able to communicate over the same local network. The API is currently intended for local-network use only.

---

## Web Interface

The source for the web interface is kept in the repository under `UI/`:

```text
UI/
├── index.html
├── style.css
└── js/
    └── api.js
```

The interface uses a black background and green terminal/Matrix-inspired styling. It displays controller connection status, current heater state, operating mode, current time, schedule information, and controls for mode, schedules, and manual operation.

The “next scheduled start” indicator is calculated by the interface using the controller's reported time and configured schedule. If the controller's time is not synchronized, the interface indicates that rather than displaying a misleading time.

### UI server

The interface is currently deployed on a Samsung laptop running Ubuntu Server. The systemd service is named:

```text
water-heater-ui.service
```

It serves the static UI files on port `8081`. From a device on the same local network, open:

```text
http://192.168.1.50:8081
```

This address reflects the current server setup and may change unless a DHCP reservation is configured on the router.

The UI server is separate from the ESP8266. Restarting or updating the UI server does not require uploading firmware to the ESP8266. Conversely, firmware changes require uploading the firmware to the controller.

---

## Deploying the Web Interface

The repository on the development computer is the source of truth. The copy under `/home/alva/water-heater/UI/` on the Ubuntu Server laptop is the deployed copy; changing a file on one machine does not automatically update the other.

From the project repository on the development computer, deploy an individual changed file with `scp`. For example, to update the main HTML file:

```bash
scp UI/index.html alva@192.168.1.50:/home/alva/water-heater/UI/index.html
```

To deploy the JavaScript or stylesheet, use the corresponding source and destination paths. For example:

```bash
scp UI/js/api.js alva@192.168.1.50:/home/alva/water-heater/UI/js/api.js
scp UI/style.css alva@192.168.1.50:/home/alva/water-heater/UI/style.css
```

These commands assume the server's current LAN address and directory layout. Adjust the address if the server's IP changes.

After deployment:

1. Open or refresh the interface at `http://192.168.1.50:8081`.
2. Check the browser's connection indicator.
3. Verify that the reported time, mode, schedule, and relay state match the controller.
4. Test controls carefully while the household water heater remains disconnected.

A static HTML/CSS/JavaScript update normally does not require restarting the UI service. A browser refresh may be needed to load the updated file.

---

## Current Firmware Behavior

The firmware is responsible for the controller's local operation. At a high level, it initializes the relay and network, synchronizes time when possible, loads persistent configuration, processes API requests, evaluates the selected control mode and schedule, updates the relay, and prints diagnostic information over serial.

The exact startup and loop sequence may change as the firmware is refactored. The essential design principle is that schedule evaluation and relay control remain inside the ESP8266 rather than depending on a browser being open.

---

## Persistence and Offline Behavior

Schedule and operating-mode configuration are stored persistently on the ESP8266, so these settings should survive a controller restart or power interruption.

The web interface and its server are not required for the ESP8266 to execute its local control logic. However, the controller still depends on successful time synchronization to know the current time accurately; behavior during loss of time synchronization should be tested and documented as the firmware evolves.

---

## Known Limitations and Work in Progress

- **Remote access:** The current interface and API are intended for the local network. Internet access has not been implemented.
- **Central backend:** A Go REST API on the Ubuntu Server laptop is planned, but the browser currently communicates directly with the ESP8266 API.
- **Network reliability:** DHCP address changes can break configured URLs. Router DHCP reservations are a planned configuration improvement.
- **Wi-Fi recovery:** Wi-Fi disconnection and reconnection behavior should be tested and improved.
- **Time synchronization:** NTP failure and recovery behavior should be tested, especially to ensure safe schedule behavior when the clock is not valid.
- **Temperature measurement:** No temperature sensor is currently integrated. Any temperature shown as a placeholder must not be treated as a real measurement.
- **Electrical integration:** The household water heater is still disconnected from the controller.

---

## Development Plan

The project is developed incrementally, with changes tested before being committed.

### Firmware robustness

- Improve Wi-Fi reconnection and timeout behavior.
- Improve NTP synchronization recovery.
- Review safe startup and relay states.
- Reduce unnecessary blocking behavior.
- Improve error reporting.
- Keep application logic separate from hardware control.

### API and web interface

- Keep the interface aligned with the current ESP8266 API.
- Improve status and error feedback when the controller is unreachable.
- Continue testing mode changes, schedule updates, and manual controls.

### Centralized communication (planned)

The next architectural step is to add a Go REST API on the Ubuntu Server laptop. The intended future flow is:

```text
Browser UI
    ↓
Go REST API on server
    ↓
ESP8266 HTTP API on local network
    ↓
Local control logic and relay
```

This is a planned change, not the current deployed architecture. The ESP8266 should continue to operate locally if the server or browser is unavailable.

### Future features

Possible later additions include temperature measurement, history, a database, user accounts, and remote access through a secure architecture. These features are not part of the current implementation.

---

## Development Workflow

The repository uses Git for version control:

- `main` represents a stable baseline.
- `develop` is used for ongoing development and testing.

Preferred workflow:

```text
Edit
  ↓
Compile / inspect changes
  ↓
Upload firmware if firmware changed
  ↓
Deploy UI files if UI changed
  ↓
Test on the local network
  ↓
Commit
  ↓
Push to develop
```

Only upload firmware when firmware files have changed. UI-only changes can be deployed to the Ubuntu Server laptop without reflashing the ESP8266.

### Firmware upload

Firmware is uploaded through the Arduino IDE while the controller is placed in programming mode. After uploading, return the controller to execution mode, reset it if necessary, and verify startup through the serial monitor or API.

---

## Project Structure

The repository is organized approximately as follows:

```text
esp8266-water-heater-controller/
├── README.md
├── UI/
│   ├── index.html
│   ├── style.css
│   └── js/
│       └── api.js
└── esp8266_water_heater/
    └── esp8266_water_heater.ino
```

A local `secrets.h` file is also required for firmware builds, but it is excluded from the public repository and should not be committed. The current UI files are not stored in the ESP8266's LittleFS filesystem.

---

## Safety

The controller is intended to eventually control a household electric water heater. The actual high-voltage electrical installation and its safety mechanisms are outside the scope of the software.

During development, the water heater remains disconnected while the firmware, relay, network communication, and control logic are tested. Before connecting the appliance, hardware modifications, electrical protection, relay ratings, isolation, grounding, enclosure, and all other electrical safety requirements must be independently verified by a qualified person.

---

## License

License to be defined.
