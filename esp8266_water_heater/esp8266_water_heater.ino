
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
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
  MODE_MANUAL
  // MODE_AUTO
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
// WEB API SERVER
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


bool clockSynchronized = false;

// ============================================================
// FUNCTION: formatTime
// ============================================================

String formatTime(uint16_t minutes)
{
  char buffer[6];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02u:%02u",
    minutes / 60,
    minutes % 60
  );

  return String(buffer);
}

// ============================================================
// FUNCTION: formatCurrentTime
// ============================================================

String formatCurrentTime()
{
  char buffer[9];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02u:%02u:%02u",
    currentTime.hour,
    currentTime.minute,
    currentTime.second
  );

  return String(buffer);
}


// ============================================================
// FUNCTION: setRelay
// ============================================================

void setRelay(bool turnOn)
{
  // This assumes an active-LOW relay module.
  digitalWrite(
    RELAY_PIN,
    turnOn ? LOW : HIGH
  );

  waterHeaterState.relayIsOn = turnOn;
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
 // API HANDLER: GET /api/status
 // ============================================================

void handleStatus()
{
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool scheduleActive =
    shouldHeatAutomatically(currentMinutes);

  String json = "{";

  json += "\"relayIsOn\":";
  json += waterHeaterState.relayIsOn ? "true" : "false";

  json += ",\"mode\":\"";
  json += waterHeaterConfig.controlMode == MODE_AUTO
    ? "AUTO"
    : "MANUAL";
  json += "\"";

  json += ",\"manualRelayState\":";
  json += waterHeaterState.manualRelayState ? "true" : "false";

  json += ",\"time\":\"";
  json += formatCurrentTime();
  json += "\"";

  json += ",\"timeValid\":";
  json += clockSynchronized ? "true" : "false";

  json += ",\"scheduleActive\":";
  json += scheduleActive ? "true" : "false";

  json += ",\"schedules\":[";

  for (
    uint8_t i = 0;
    i < waterHeaterConfig.scheduleCount;
    i++
  )
  {
    if (i > 0)
    {
      json += ",";
    }

    json += "{";

    json += "\"start\":\"";
    json += formatTime(
      waterHeaterConfig.schedules[i].startMinutes
    );
    json += "\",";

    json += "\"end\":\"";
    json += formatTime(
      waterHeaterConfig.schedules[i].endMinutes
    );
    json += "\"";
    json += "}";
  }

  json += "]}";

  server.send(
    200,
    "application/json",
    json
  );
}


// ============================================================
// API HANDLER: POST /api/control
// ============================================================

// Handle Mode (Auto or Manual)
void handleMode()
{
  if (!server.hasArg("mode"))
  {
    server.send(400, "application/json",
                "{\"error\":\"Falta el parametro mode\"}");
    return;
  }

  String requestedMode = server.arg("mode");

  if (requestedMode == "AUTO")
  {
    // No activar el modo automatico sin hora sincronizada.
    if (!clockSynchronized)
    {
      server.send(409, "application/json",
                  "{\"error\":\"El reloj todavia no esta sincronizado\"}");
      return;
    }

    waterHeaterConfig.controlMode = MODE_AUTO;
  }
  else if (requestedMode == "MANUAL")
  {
    waterHeaterConfig.controlMode = MODE_MANUAL;

    // Al entrar en modo manual, comenzar con el rele apagado.
    waterHeaterState.manualRelayState = false;
  }
  else
  {
    server.send(400, "application/json",
                "{\"error\":\"Modo invalido\"}");
    return;
  }

  updateRelay();

  server.send(200, "application/json",
              "{\"ok\":true}");
}

void handleControl()
{
  // Manual control is only allowed in MANUAL mode.
  if (waterHeaterConfig.controlMode != MODE_MANUAL)
  {
    server.send(
      409,
      "text/plain",
      "Control manual no disponible en modo AUTO"
    );

    return;
  }

  // Check that the "state" parameter was received.
  if (!server.hasArg("state"))
  {
    server.send(
      400,
      "text/plain",
      "Falta el parametro 'state'"
    );

    return;
  }

  String requestedState = server.arg("state");

  if (requestedState == "on")
  {
    waterHeaterState.manualRelayState = true;
    setRelay(true);

    server.send(
      200,
      "text/plain",
      "Relay ON"
    );

    return;
  }

  if (requestedState == "off")
  {
    waterHeaterState.manualRelayState = false;
    setRelay(false);

    server.send(
      200,
      "text/plain",
      "Relay OFF"
    );

    return;
  }

  // Invalid state value.
  server.send(
    400,
    "text/plain",
    "Valor de 'state' invalido. Use 'on' o 'off'"
  );
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(9600);

  // Initialize relay.
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

  // ==========================================================
  // WI-FI
  // ==========================================================

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado");

  // ==========================================================
  // NTP
  // ==========================================================

  timeClient.begin();

  // ==========================================================
  // WEB API
  // ==========================================================

  server.on("/api/status",HTTP_GET,handleStatus);
  server.on("/api/control",HTTP_POST,handleControl);
  server.on("/api/mode", HTTP_POST, handleMode);

  server.enableCORS(true);
  server.begin();

  Serial.println("API web iniciada");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // Handle incoming HTTP requests as often as possible.
  server.handleClient();

  // Update the clock and relay every 5 seconds.
  static unsigned long previousUpdate = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousUpdate >= LOOP_DELAY_MS)
  {
    previousUpdate = currentMillis;

    if (timeClient.update())
    {
      clockSynchronized = true;
    }

    updateCurrentTime();
    updateRelay();
    printSystemStatus();
  }
}

// ============================================================
// RELAY TESTING LOOP
// ============================================================

/*
void loop()
{
  setRelay(true);
  Serial.println("Relay: ON");

  delay(3000);

  setRelay(false);
  Serial.println("Relay: OFF");

  delay(3000);
}
*/
