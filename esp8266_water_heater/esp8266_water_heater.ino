
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#include "secrets.h"

// ============================================================
// HARDWARE CONFIGURATION
// ============================================================

const uint8_t RELAY_PIN = 0;

// El modulo de rele utilizado se considera activo en LOW.
// Revisar el circuito antes de conectar la carga real.

// ============================================================
// SYSTEM CONFIGURATION
// ============================================================

const long UTC_OFFSET_SECONDS = -10800;
const unsigned long LOOP_DELAY_MS = 5000;

// ============================================================
// PERSISTENT CONFIGURATION
// ============================================================

const uint32_t CONFIG_MAGIC = 0x57484331;
const uint8_t CONFIG_VERSION = 1;
const size_t EEPROM_SIZE = 512;

// ============================================================
// TYPES
// ============================================================

struct Schedule {
  uint16_t startMinutes;
  uint16_t endMinutes;
};

enum ControlMode {
  MODE_AUTO,
  MODE_MANUAL
};

// ============================================================
// WATER HEATER CONFIGURATION
// ============================================================

struct WaterHeaterConfig {
  Schedule schedules[4];
  uint8_t scheduleCount;
  ControlMode controlMode;
};

WaterHeaterConfig waterHeaterConfig = {
  { { 360, 420 },    // 06:00 - 07:00
    { 1080, 1140 },  // 18:00 - 19:00
    { 0, 0 },
    { 0, 0 } },

  2,
  MODE_MANUAL
  // MODE_AUTO
};

// ============================================================
// WATER HEATER STATE
// ============================================================

struct WaterHeaterState {
  bool manualRelayState;
  bool relayIsOn;
};

WaterHeaterState waterHeaterState = {
  false,
  false
};

// ============================================================
// PERSISTENT DATA FORMAT
// ============================================================

struct PersistentConfig {
  uint32_t magic;
  uint8_t version;
  uint8_t scheduleCount;
  Schedule schedules[4];
  uint8_t controlMode;
};

// ============================================================
// NTP
// ============================================================

WiFiUDP ntpUDP;

NTPClient timeClient(
  ntpUDP,
  "pool.ntp.org",
  UTC_OFFSET_SECONDS);

// ============================================================
// WEB API SERVER
// ============================================================

ESP8266WebServer server(80);

// ============================================================
// CURRENT TIME
// ============================================================

struct CurrentTime {
  byte hour;
  byte minute;
  byte second;

  uint16_t minutesSinceMidnight() const {
    return hour * 60 + minute;
  }
};

CurrentTime currentTime = { 0, 0, 0 };

bool clockSynchronized = false;

// ============================================================
// FUNCTION: formatTime
// ============================================================

String formatTime(uint16_t minutes) {
  char buffer[6];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02u:%02u",
    minutes / 60,
    minutes % 60);

  return String(buffer);
}

// ============================================================
// FUNCTION: formatCurrentTime
// ============================================================

String formatCurrentTime() {
  char buffer[9];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02u:%02u:%02u",
    currentTime.hour,
    currentTime.minute,
    currentTime.second);

  return String(buffer);
}

// ============================================================
// FUNCTION: setRelay
// ============================================================

void setRelay(bool turnOn) {
  // Assumes an active-LOW relay module.
  digitalWrite(
    RELAY_PIN,
    turnOn ? LOW : HIGH);

  waterHeaterState.relayIsOn = turnOn;
}

// ============================================================
// FUNCTION: isScheduleActive
// ============================================================

bool isScheduleActive(
  const Schedule& schedule,
  uint16_t currentMinutes) {
  return (
    currentMinutes >= schedule.startMinutes && currentMinutes < schedule.endMinutes);
}

// ============================================================
// FUNCTION: shouldHeatAutomatically
// ============================================================

bool shouldHeatAutomatically(uint16_t currentMinutes) {
  for (
    uint8_t i = 0;
    i < waterHeaterConfig.scheduleCount;
    i++) {
    if (
      isScheduleActive(
        waterHeaterConfig.schedules[i],
        currentMinutes)) {
      return true;
    }
  }

  return false;
}

// ============================================================
// FUNCTION: shouldRelayBeOn
// ============================================================

bool shouldRelayBeOn(uint16_t currentMinutes) {
  if (waterHeaterConfig.controlMode == MODE_MANUAL) {
    return waterHeaterState.manualRelayState;
  }

  // Never activate automatic heating without a valid clock.
  if (!clockSynchronized) {
    return false;
  }

  return shouldHeatAutomatically(currentMinutes);
}

// ============================================================
// FUNCTION: updateCurrentTime
// ============================================================

void updateCurrentTime() {
  currentTime.hour = timeClient.getHours();
  currentTime.minute = timeClient.getMinutes();
  currentTime.second = timeClient.getSeconds();
}

// ============================================================
// FUNCTION: updateRelay
// ============================================================

void updateRelay() {
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool relayShouldBeOn =
    shouldRelayBeOn(currentMinutes);

  setRelay(relayShouldBeOn);
}

// ============================================================
// FUNCTION: printSystemStatus
// ============================================================

void printSystemStatus() {
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool scheduleActive =
    clockSynchronized && shouldHeatAutomatically(currentMinutes);

  Serial.print("Time: ");

  if (clockSynchronized) {
    Serial.print(formatCurrentTime());
  } else {
    Serial.print("NOT SYNCED");
  }

  Serial.print(" | Relay: ");
  Serial.print(
    waterHeaterState.relayIsOn ? "ON" : "OFF");

  Serial.print(" | Mode: ");
  Serial.print(
    waterHeaterConfig.controlMode == MODE_AUTO
      ? "AUTO"
      : "MANUAL");

  Serial.print(" | Schedule: ");
  Serial.println(
    scheduleActive ? "ACTIVE" : "INACTIVE");
}

// ============================================================
// FUNCTION: isValidPersistentConfig
// ============================================================

bool isValidPersistentConfig(const PersistentConfig& config) {
  if (
    config.magic != CONFIG_MAGIC || config.version != CONFIG_VERSION || config.scheduleCount < 1 || config.scheduleCount > 4 || config.controlMode > MODE_MANUAL) {
    return false;
  }

  for (uint8_t i = 0; i < config.scheduleCount; i++) {
    uint16_t start =
      config.schedules[i].startMinutes;

    uint16_t end =
      config.schedules[i].endMinutes;

    if (start >= end || end > 1439) {
      return false;
    }

    for (uint8_t j = 0; j < i; j++) {
      bool overlaps =
        start < config.schedules[j].endMinutes && config.schedules[j].startMinutes < end;

      if (overlaps) {
        return false;
      }
    }
  }

  return true;
}

// ============================================================
// FUNCTION: saveConfiguration
// ============================================================

bool saveConfiguration() {
  PersistentConfig config = {};

  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;
  config.scheduleCount =
    waterHeaterConfig.scheduleCount;

  for (uint8_t i = 0; i < 4; i++) {
    config.schedules[i] =
      waterHeaterConfig.schedules[i];
  }

  config.controlMode =
    static_cast<uint8_t>(
      waterHeaterConfig.controlMode);

  EEPROM.put(0, config);

  if (!EEPROM.commit()) {
    Serial.println(
      "ERROR: No se pudo guardar la configuracion");

    return false;
  }

  Serial.println(
    "Configuracion guardada en memoria");

  return true;
}

// ============================================================
// FUNCTION: loadConfiguration
// ============================================================

bool loadConfiguration() {
  PersistentConfig config = {};

  EEPROM.get(0, config);

  if (!isValidPersistentConfig(config)) {
    Serial.println(
      "No hay configuracion valida guardada; "
      "se usan los valores iniciales del firmware");

    return false;
  }

  waterHeaterConfig.scheduleCount =
    config.scheduleCount;

  for (uint8_t i = 0; i < 4; i++) {
    waterHeaterConfig.schedules[i] =
      config.schedules[i];
  }

  waterHeaterConfig.controlMode =
    static_cast<ControlMode>(
      config.controlMode);

  // Never restore a previous manual ON state.
  waterHeaterState.manualRelayState = false;

  Serial.println(
    "Configuracion cargada desde memoria");

  return true;
}

// ============================================================
// API HANDLER: GET /api/status
// ============================================================

void handleStatus() {
  uint16_t currentMinutes =
    currentTime.minutesSinceMidnight();

  bool scheduleActive =
    clockSynchronized && shouldHeatAutomatically(currentMinutes);

  String json = "{";

  json += "\"relayIsOn\":";
  json += waterHeaterState.relayIsOn ? "true" : "false";

  json += ",\"mode\":\"";
  json +=
    waterHeaterConfig.controlMode == MODE_AUTO
      ? "AUTO"
      : "MANUAL";
  json += "\"";

  json += ",\"manualRelayState\":";
  json +=
    waterHeaterState.manualRelayState
      ? "true"
      : "false";

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
    i++) {
    if (i > 0) {
      json += ",";
    }

    json += "{";

    json += "\"start\":\"";
    json += formatTime(
      waterHeaterConfig.schedules[i].startMinutes);
    json += "\",";

    json += "\"end\":\"";
    json += formatTime(
      waterHeaterConfig.schedules[i].endMinutes);
    json += "\"";

    json += "}";
  }

  json += "]}";

  server.send(
    200,
    "application/json",
    json);
}

// ============================================================
// API HANDLER: POST /api/mode
// ============================================================

void handleMode() {
  if (!server.hasArg("mode")) {
    server.send(
      400,
      "application/json",
      "{\"error\":\"Falta el parametro mode\"}");

    return;
  }

  String requestedMode = server.arg("mode");

  if (
    requestedMode == "AUTO" && !clockSynchronized) {
    server.send(
      409,
      "application/json",
      "{\"error\":\"El reloj todavia no esta sincronizado\"}");

    return;
  }

  if (
    requestedMode != "AUTO" && requestedMode != "MANUAL") {
    server.send(
      400,
      "application/json",
      "{\"error\":\"Modo invalido\"}");

    return;
  }

  ControlMode previousMode =
    waterHeaterConfig.controlMode;

  bool previousManualState =
    waterHeaterState.manualRelayState;

  if (requestedMode == "AUTO") {
    waterHeaterConfig.controlMode = MODE_AUTO;
  } else {
    waterHeaterConfig.controlMode = MODE_MANUAL;

    // Enter manual mode with the relay OFF.
    waterHeaterState.manualRelayState = false;
  }

  if (!saveConfiguration()) {
    waterHeaterConfig.controlMode = previousMode;

    waterHeaterState.manualRelayState =
      previousManualState;

    updateRelay();

    server.send(
      500,
      "application/json",
      "{\"error\":\"No se pudo guardar la configuracion\"}");

    return;
  }

  updateRelay();

  server.send(
    200,
    "application/json",
    "{\"ok\":true}");
}

// ============================================================
// FUNCTION: parseTimeValue
// ============================================================

bool parseTimeValue(
  String value,
  uint16_t& minutes) {
  // Expected format: HH:MM.
  if (
    value.length() != 5 || value.charAt(2) != ':') {
    return false;
  }

  if (
    value.charAt(0) < '0' || value.charAt(0) > '9' || value.charAt(1) < '0' || value.charAt(1) > '9' || value.charAt(3) < '0' || value.charAt(3) > '9' || value.charAt(4) < '0' || value.charAt(4) > '9') {
    return false;
  }

  uint8_t hour =
    (value.charAt(0) - '0') * 10 + (value.charAt(1) - '0');

  uint8_t minute =
    (value.charAt(3) - '0') * 10 + (value.charAt(4) - '0');

  if (hour > 23 || minute > 59) {
    return false;
  }

  minutes = hour * 60 + minute;

  return true;
}

// ============================================================
// API HANDLER: POST /api/schedules
// ============================================================

void handleSchedules() {
  if (!server.hasArg("count")) {
    server.send(
      400,
      "application/json",
      "{\"error\":\"Falta la cantidad de horarios\"}");

    return;
  }

  String countValue = server.arg("count");

  if (
    countValue.length() != 1 || countValue.charAt(0) < '1' || countValue.charAt(0) > '4') {
    server.send(
      400,
      "application/json",
      "{\"error\":\"Debe haber entre 1 y 4 horarios\"}");

    return;
  }

  uint8_t requestedCount =
    countValue.toInt();

  Schedule requestedSchedules[4];

  // Validate all requested schedules first.
  for (uint8_t i = 0; i < requestedCount; i++) {
    String startName = "start" + String(i);
    String endName = "end" + String(i);

    if (
      !server.hasArg(startName) || !server.hasArg(endName)) {
      server.send(
        400,
        "application/json",
        "{\"error\":\"Falta una hora de inicio o fin\"}");

      return;
    }

    if (
      !parseTimeValue(
        server.arg(startName),
        requestedSchedules[i].startMinutes)
      || !parseTimeValue(
        server.arg(endName),
        requestedSchedules[i].endMinutes)) {
      server.send(
        400,
        "application/json",
        "{\"error\":\"Formato de hora invalido\"}");

      return;
    }

    // Overnight schedules are not supported.
    if (
      requestedSchedules[i].startMinutes >= requestedSchedules[i].endMinutes) {
      server.send(
        400,
        "application/json",
        "{\"error\":\"El inicio debe ser anterior al fin\"}");

      return;
    }

    // Reject overlapping schedules.
    for (uint8_t j = 0; j < i; j++) {
      bool overlaps =
        requestedSchedules[i].startMinutes < requestedSchedules[j].endMinutes && requestedSchedules[j].startMinutes < requestedSchedules[i].endMinutes;

      if (overlaps) {
        server.send(
          400,
          "application/json",
          "{\"error\":\"Los horarios no pueden superponerse\"}");

        return;
      }
    }
  }

  // Keep a copy so a failed flash write can be rolled back.
  Schedule previousSchedules[4];

  uint8_t previousCount =
    waterHeaterConfig.scheduleCount;

  for (uint8_t i = 0; i < 4; i++) {
    previousSchedules[i] =
      waterHeaterConfig.schedules[i];
  }

  // Apply the validated configuration.
  for (uint8_t i = 0; i < requestedCount; i++) {
    waterHeaterConfig.schedules[i] =
      requestedSchedules[i];
  }

  waterHeaterConfig.scheduleCount =
    requestedCount;

  if (!saveConfiguration()) {
    for (uint8_t i = 0; i < 4; i++) {
      waterHeaterConfig.schedules[i] =
        previousSchedules[i];
    }

    waterHeaterConfig.scheduleCount =
      previousCount;

    updateRelay();

    server.send(
      500,
      "application/json",
      "{\"error\":\"No se pudo guardar la configuracion\"}");

    return;
  }

  updateRelay();

  server.send(
    200,
    "application/json",
    "{\"ok\":true}");
}

// ============================================================
// API HANDLER: POST /api/control
// ============================================================

void handleControl() {
  // Manual control is only allowed in MANUAL mode.
  if (waterHeaterConfig.controlMode != MODE_MANUAL) {
    server.send(
      409,
      "text/plain",
      "Control manual no disponible en modo AUTO");

    return;
  }

  if (!server.hasArg("state")) {
    server.send(
      400,
      "text/plain",
      "Falta el parametro 'state'");

    return;
  }

  String requestedState = server.arg("state");

  if (requestedState == "on") {
    waterHeaterState.manualRelayState = true;
    setRelay(true);

    server.send(
      200,
      "text/plain",
      "Relay ON");

    return;
  }

  if (requestedState == "off") {
    waterHeaterState.manualRelayState = false;
    setRelay(false);

    server.send(
      200,
      "text/plain",
      "Relay OFF");

    return;
  }

  server.send(
    400,
    "text/plain",
    "Valor de 'state' invalido. Use 'on' o 'off'");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(9600);

  // Configure the relay and establish the initial OFF state.
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);

  // Initialize emulated EEPROM and load saved configuration.
  EEPROM.begin(EEPROM_SIZE);
  loadConfiguration();

  /*
  // Relay startup test.
  // Keep disabled during normal operation.

  delay(2000);
  setRelay(true);

  delay(2000);
  setRelay(false);
  */

  // ==========================================================
  // WI-FI
  // ==========================================================

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    // Keep the relay OFF while waiting for Wi-Fi.
    setRelay(false);

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado");

  // ==========================================================
  // NTP
  // ==========================================================

  timeClient.begin();

  // Attempt initial synchronization instead of waiting
  // for the periodic update interval.
  if (timeClient.forceUpdate()) {
    clockSynchronized = true;
    updateCurrentTime();

    Serial.println("Hora sincronizada correctamente");
  } else {
    clockSynchronized = false;

    Serial.println(
      "ADVERTENCIA: no se pudo sincronizar la hora");
  }

  // Do not activate AUTO mode until the clock is valid.
  updateRelay();

  // ==========================================================
  // WEB API
  // ==========================================================

  server.on(
    "/api/status",
    HTTP_GET,
    handleStatus);

  server.on(
    "/api/control",
    HTTP_POST,
    handleControl);

  server.on(
    "/api/mode",
    HTTP_POST,
    handleMode);

  server.on(
    "/api/schedules",
    HTTP_POST,
    handleSchedules);

  server.enableCORS(true);
  server.begin();

  Serial.println("API web iniciada");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  printSystemStatus();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Handle incoming HTTP requests as often as possible.
  server.handleClient();

  static unsigned long previousUpdate = 0;

  unsigned long currentMillis = millis();

  if (
    currentMillis - previousUpdate >= LOOP_DELAY_MS) {
    previousUpdate = currentMillis;

    // Retry synchronization until a valid time is obtained.
    if (!clockSynchronized) {
      if (timeClient.forceUpdate()) {
        clockSynchronized = true;

        Serial.println(
          "Hora sincronizada correctamente");
      }
    } else {
      // Keep the clock updated when possible.
      timeClient.update();
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
