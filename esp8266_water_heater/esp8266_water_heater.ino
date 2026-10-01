#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "secrets.h"


// ============================================================
// HARDWARE CONFIGURATION
// ============================================================

const uint8_t RELAY_PIN = 0;


// ============================================================
// SYSTEM CONFIGURATION
// ============================================================

const long UTC_OFFSET_SECONDS = -10800;

const unsigned long LOOP_DELAY_MS = 5000;


// ============================================================
// TYPES
// ============================================================

// Represents one automatic heating schedule.
//
// Times are expressed as minutes since midnight.
//
// Example:
// 06:00 -> 360
// 07:00 -> 420

struct Schedule
{
  uint16_t startMinutes;
  uint16_t endMinutes;
};


// Represents the control mode of the water heater.

enum ControlMode
{
  MODE_AUTO,
  MODE_MANUAL
};


// ============================================================
// WATER HEATER CONFIGURATION
// ============================================================

// Configuration that can eventually be modified
// through the local web interface.

struct WaterHeaterConfig
{
  Schedule schedules[4];
  uint8_t scheduleCount;

  ControlMode controlMode;
};


// Current water heater configuration.

WaterHeaterConfig waterHeaterConfig =
{
  {
    {360, 420},       // 06:00 - 07:00
    {1080, 1140}      // 18:00 - 19:00
  },

  2,                  // Number of active schedules

  MODE_AUTO           // Control mode
};


// ============================================================
// WATER HEATER STATE
// ============================================================

// Represents the current operational state of the
// water heater.
//
// Some of these values will eventually be exposed
// through the web API.

struct WaterHeaterState
{
  bool manualRelayState;
  bool relayIsOn;
};


// Current water heater state.

WaterHeaterState waterHeaterState =
{
  false,              // Manual relay state
  false               // Relay state
};


// ============================================================
// NTP
// ============================================================

WiFiUDP ntpUDP;

NTPClient timeClient(
  ntpUDP,
  "pool.ntp.org",
  UTC_OFFSET_SECONDS
);


// ============================================================
// CURRENT TIME
// ============================================================

struct CurrentTime
{
  byte hour;
  byte minute;
  byte second;

  uint16_t minutesSinceMidnight() const
  {
    return hour * 60 + minute;
  }
};

CurrentTime currentTime;


// ============================================================
// TEST / SERIAL DATA
// ============================================================

// These variables and functions are kept for now because
// they are part of the original test code.

int serialIndex;
byte serialData[20];


// ============================================================
// FUNCTION: setRelay
// ============================================================

// Controls the physical relay.
//
// The relay module uses inverted logic:
//
// ON  -> GPIO LOW
// OFF -> GPIO HIGH
//
// The rest of the program should only use:
// setRelay(true)  -> ON
// setRelay(false) -> OFF

void setRelay(bool turnOn)
{
  digitalWrite(
    RELAY_PIN,
    turnOn ? LOW : HIGH
  );

  waterHeaterState.relayIsOn = turnOn;
}


// ============================================================
// FUNCTION: checkSerial
// ============================================================

void checkSerial()
{
  byte receptionFinished = 0;

  serialIndex = 0;

  if (Serial.available() > 0)
  {
    do
    {
      serialData[serialIndex] = Serial.read();

      if (serialData[serialIndex] == 61)  // '='
      {
        receptionFinished = 1;
      }

      ++serialIndex;

    } while (receptionFinished == 0);

    Serial.print(serialIndex - 1, DEC);
  }
}


// ============================================================
// FUNCTION: isScheduleActive
// ============================================================

// Returns true when the specified schedule is active.

bool isScheduleActive(
  const Schedule& schedule,
  uint16_t currentMinutes
)
{
  return (
    currentMinutes >= schedule.startMinutes &&
    currentMinutes < schedule.endMinutes
  );
}


// ============================================================
// FUNCTION: shouldHeatAutomatically
// ============================================================

// Determines whether the automatic schedule requires
// the water heater to be ON.

bool shouldHeatAutomatically(uint16_t currentMinutes)
{
  for (
    uint8_t i = 0;
    i < waterHeaterConfig.scheduleCount;
    i++
  )
  {
    if (
      isScheduleActive(
        waterHeaterConfig.schedules[i],
        currentMinutes
      )
    )
    {
      return true;
    }
  }

  return false;
}


// ============================================================
// FUNCTION: shouldRelayBeOn
// ============================================================

// Determines the desired water heater state based on
// the current control mode.

bool shouldRelayBeOn(uint16_t currentMinutes)
{
  if (waterHeaterConfig.controlMode == MODE_MANUAL)
  {
    return waterHeaterState.manualRelayState;
  }

  return shouldHeatAutomatically(currentMinutes);
}


// ============================================================
// FUNCTION: updateCurrentTime
// ============================================================
// Single entity : Time (Not independent global variables)

void updateCurrentTime()
{
  currentTime.hour = timeClient.getHours();
  currentTime.minute = timeClient.getMinutes();
  currentTime.second = timeClient.getSeconds();
}

// ============================================================
// FUNCTION: updateRelay
// ============================================================

// Updates the physical relay according to the current
// configuration and time.

void updateRelay()
{
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool relayShouldBeOn =
    shouldRelayBeOn(currentMinutes);

  setRelay(relayShouldBeOn);
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);

  // Initial state: OFF.
  setRelay(false);

  /*
  // Relay startup test.

  delay(2000);
  setRelay(true);

  delay(2000);
  setRelay(false);
  */

  // Connect to Wi-Fi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Start NTP client.
  timeClient.begin();
}


void printSystemStatus()
{
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool scheduleActive =
    shouldHeatAutomatically(currentMinutes);

  Serial.print("Time: ");
  Serial.print(currentTime.hour);
  Serial.print(":");
  Serial.print(currentTime.minute);
  Serial.print(":");
  Serial.print(currentTime.second);

  Serial.print(" | Relay: ");
  Serial.print(
    waterHeaterState.relayIsOn
      ? "ON"
      : "OFF"
  );

  Serial.print(" | Mode: ");
  Serial.print(
    waterHeaterConfig.controlMode == MODE_AUTO
      ? "AUTO"
      : "MANUAL"
  );

  Serial.print(" | Schedule: ");
  Serial.println(
    scheduleActive
      ? "ACTIVE"
      : "INACTIVE"
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  timeClient.update();

  updateCurrentTime();

  updateRelay();

  printSystemStatus();

  delay(LOOP_DELAY_MS);
}