#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ============================================================
//  CONFIGURATION
// ============================================================

const char* WIFI_SSID = "SSID";
const char* WIFI_PASSWORD = "PASSWORD";

const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32-abolfazl-02";

const char* OFFLINE_PAYLOAD = "{\"status\":\"offline\"}";

// --- Publisher topics ---
const char* TOPIC_TEMP_DATA     = "home/esp32-01/temp/data";
const char* TOPIC_STATUS        = "home/esp32-01/status";
const char* TOPIC_BUTTON_EVENT  = "home/esp32-01/button/event";
const char* TOPIC_LED_STATE     = "home/esp32-01/led/state";

// --- Subscriber topics ---
const char* TOPIC_LED_SET       = "home/esp32-01/led/set";
const char* TOPIC_INTERVAL_SET  = "home/esp32-01/interval/set";

// --- Timing constants ---
constexpr unsigned long MIN_RECONNECT_DELAY = 1000;
constexpr unsigned long MAX_RECONNECT_DELAY = 30000;
constexpr unsigned long DEBOUNCE_DELAY       = 50;
constexpr unsigned long WIFI_CONNECT_TIMEOUT = 15000; // 15s, then give up and retry

// --- Pins ---
constexpr uint8_t BOOT_BUTTON_PIN = 0;

// ============================================================
//  GLOBAL STATE
// ============================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long publishInterval = 5000;
unsigned long lastPublishTime = 0;

unsigned long reconnectDelay = MIN_RECONNECT_DELAY;
unsigned long lastReconnectAttempt = 0;

bool buttonState = HIGH;
bool lastReading = HIGH;
unsigned long lastDebounceTime = 0;

// ============================================================
//  PUBLISH HELPERS
// ============================================================

void publishTemperature() {
  const float temperature = temperatureRead();

  StaticJsonDocument<128> jsonDoc;
  jsonDoc["value"] = temperature;
  jsonDoc["unit"] = "C";
  jsonDoc["timestamp"] = millis();

  char payload[128]{};
  serializeJson(jsonDoc, payload);

  mqttClient.publish(TOPIC_TEMP_DATA, payload);

  Serial.printf("[TEMP] %s\n", payload);
}

void publishStatus(const String& status) {
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["status"] = status;
  jsonDoc["timestamp"] = millis();

  char payload[128]{};
  serializeJson(jsonDoc, payload);

  mqttClient.publish(TOPIC_STATUS, payload, true /* retained */);

  Serial.printf("[STATUS] %s\n", payload);
}

void publishButtonEvent() {
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["event"] = "button_pressed";
  jsonDoc["timestamp"] = millis();

  char payload[128]{};
  serializeJson(jsonDoc, payload);

  mqttClient.publish(TOPIC_BUTTON_EVENT, payload);

  Serial.printf("[BUTTON] %s\n", payload);
}

void publishLedState(const String& state) {
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["state"] = state;
  jsonDoc["timestamp"] = millis();

  char payload[128]{};
  serializeJson(jsonDoc, payload);

  mqttClient.publish(TOPIC_LED_STATE, payload, true /* retained */);

  Serial.printf("[LED] %s\n", payload);
}

// ============================================================
//  MQTT CALLBACK
// ============================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.printf("[MQTT-IN] topic=%s payload=%s\n", topic, message.c_str());

  if (strcmp(topic, TOPIC_LED_SET) == 0) {
    if (message == "ON") {
      digitalWrite(LED_BUILTIN, HIGH);
      publishLedState("ON");
    } else if (message == "OFF") {
      digitalWrite(LED_BUILTIN, LOW);
      publishLedState("OFF");
    } else {
      Serial.printf("[LED] Ignored unknown command: %s\n", message.c_str());
    }
  } else if (strcmp(topic, TOPIC_INTERVAL_SET) == 0) {
    const int newIntervalSec = message.toInt();

    if (newIntervalSec > 0) {
      publishInterval = (unsigned long)newIntervalSec * 1000;
      Serial.printf("[INTERVAL] Updated to %d s\n", newIntervalSec);
    } else {
      Serial.printf("[INTERVAL] Invalid value '%s', ignored\n", message.c_str());
    }
  }
}

// ============================================================
//  WIFI
// ============================================================

/**
 * Connects to the configured WiFi network, blocking until either
 * connected or WIFI_CONNECT_TIMEOUT elapses. Returns false on
 * timeout so the caller can retry instead of hanging forever.
 */
bool connectWiFi() {
  Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttemptTime >= WIFI_CONNECT_TIMEOUT) {
      Serial.println();
      Serial.println("[WIFI] Connection timed out");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.printf("[WIFI] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// ============================================================
//  MQTT CONNECT
// ============================================================

/**
 * Establishes a connection to the MQTT broker and (re)subscribes to
 * all topics this device listens to. Subscriptions do not persist
 * across reconnects, so this must run again every time the
 * connection drops. Also republishes status/LED state so the
 * broker's retained values reflect reality after a reconnect.
 */
bool connectMQTT() {
  Serial.print("[MQTT] Connecting...");

  if (mqttClient.connect(MQTT_CLIENT_ID, nullptr, nullptr, TOPIC_STATUS, 1, true, OFFLINE_PAYLOAD)) {
    Serial.println(" connected!");

    mqttClient.subscribe(TOPIC_LED_SET);
    mqttClient.subscribe(TOPIC_INTERVAL_SET);

    publishStatus("online");
    publishLedState(digitalRead(LED_BUILTIN) ? "ON" : "OFF");

    reconnectDelay = MIN_RECONNECT_DELAY;
    return true;
  }

  Serial.printf(" failed, rc=%d\n", mqttClient.state());
  return false;
}

// ============================================================
//  SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  while (!connectWiFi()) {
    Serial.println("[WIFI] Retrying...");
  }

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection lost, reconnecting...");
    connectWiFi();
    return;
  }

  if (!mqttClient.connected()) {
    const unsigned long currentTime = millis();

    if (currentTime - lastReconnectAttempt >= reconnectDelay) {
      lastReconnectAttempt = currentTime;

      if (!connectMQTT()) {
        reconnectDelay = min(reconnectDelay * 2, MAX_RECONNECT_DELAY);
        Serial.printf("[MQTT] Next attempt in %lu ms\n", reconnectDelay);
      }
    }
    return;
  }

  mqttClient.loop();

  const unsigned long currentTime = millis();
  if (currentTime - lastPublishTime >= publishInterval) {
    publishTemperature();
    lastPublishTime = currentTime;
  }

  const bool reading = digitalRead(BOOT_BUTTON_PIN);
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        publishButtonEvent();
      }
    }
  }

  lastReading = reading;
}