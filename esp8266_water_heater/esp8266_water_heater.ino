#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>

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

struct Schedule
{
  uint16_t startMinutes;
  uint16_t endMinutes;
};

enum ControlMode
{
  MODE_AUTO,
  MODE_MANUAL
};

// ============================================================
// WATER HEATER CONFIGURATION
// ============================================================

struct WaterHeaterConfig
{
  Schedule schedules[4];
  uint8_t scheduleCount;
  ControlMode controlMode;
};

WaterHeaterConfig waterHeaterConfig =
{
  {
    {360, 420},       // 06:00 - 07:00
    {1080, 1140}      // 18:00 - 19:00
  },

  2,
  MODE_AUTO
};

// ============================================================
// WATER HEATER STATE
// ============================================================

struct WaterHeaterState
{
  bool manualRelayState;
  bool relayIsOn;
};

WaterHeaterState waterHeaterState =
{
  false,
  false
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
// WEB SERVER
// ============================================================

ESP8266WebServer server(80);

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

int serialIndex;
byte serialData[20];

// ============================================================
// FUNCTION: setRelay
// ============================================================

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

void updateCurrentTime()
{
  currentTime.hour = timeClient.getHours();
  currentTime.minute = timeClient.getMinutes();
  currentTime.second = timeClient.getSeconds();
}

// ============================================================
// FUNCTION: updateRelay
// ============================================================

void updateRelay()
{
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool relayShouldBeOn =
    shouldRelayBeOn(currentMinutes);

  setRelay(relayShouldBeOn);
}

// ============================================================
// FUNCTION: printSystemStatus
// ============================================================

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
// FUNCTION: handleRoot
// ============================================================

void handleRoot()
{
  File file = LittleFS.open("/index.html", "r");

  if (!file)
  {
    server.send(
      500,
      "text/plain",
      "No se pudo abrir index.html"
    );

    return;
  }

  server.streamFile(file, "text/html");

  file.close();
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

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();

  // ==========================================================
  // LITTLEFS
  // ==========================================================

  if (!LittleFS.begin())
  {
    Serial.println("Error al montar LittleFS");
    return;
  }

  // ==========================================================
  // WEB SERVER
  // ==========================================================

  server.on("/", handleRoot);

  server.begin();

  Serial.println("Servidor web iniciado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  server.handleClient();

  timeClient.update();

  updateCurrentTime();

  updateRelay();

  printSystemStatus();

  delay(LOOP_DELAY_MS);
}