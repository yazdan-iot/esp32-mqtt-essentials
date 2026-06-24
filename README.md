# ESP32 MQTT Essentials

A hands-on ESP32 firmware project built with **PlatformIO** that demonstrates the core concepts of MQTT in a single, practical sketch — publishing, subscribing, retained messages, Last Will and Testament (LWT), QoS, JSON payloads, and resilient reconnect logic.

This project was built as a learning exercise to put MQTT theory into practice on real hardware, not just to blink an LED.

## ✨ Features

- **Publish / Subscribe** — multiple topics for sensor data, device status, button events, and LED control
- **Retained messages** — `status` and `led/state` topics are retained so any client connecting later immediately sees the last known state
- **Last Will and Testament (LWT)** — broker automatically publishes an `offline` status if the device disconnects unexpectedly (power loss, crash, network drop)
- **JSON payloads** — structured messages via `ArduinoJson`, instead of raw strings
- **Exponential backoff reconnect** — WiFi and MQTT reconnection attempts back off from 1s up to 30s instead of hammering the broker
- **Debounced button input** — clean, bounce-free button event publishing
- **Remote control** — LED on/off and publish interval are both configurable at runtime via MQTT, no reflash required

## 🧠 MQTT Concepts Covered

| Concept | Where it's implemented |
|---|---|
| Publish | `publishTemperature()`, `publishStatus()`, `publishButtonEvent()`, `publishLedState()` |
| Subscribe | `connectMQTT()` — subscribes to `led/set` and `interval/set` |
| Retained messages | `publishStatus()` and `publishLedState()` use `retain = true` |
| Last Will and Testament (LWT) | Set in `mqttClient.connect(...)` with topic `home/esp32-01/status` and payload `{"status":"offline"}` |
| QoS | LWT published with QoS 1 |
| Reconnect strategy | `connectMQTT()` + exponential backoff in `loop()` |
| Message parsing | `mqttCallback()` |

## 📡 Topic Structure

**Published by the device:**

| Topic | Payload example | Retained |
|---|---|---|
| `home/esp32-01/temp/data` | `{"value":34.5,"unit":"C","timestamp":12345}` | No |
| `home/esp32-01/status` | `{"status":"online","timestamp":12345}` | Yes |
| `home/esp32-01/button/event` | `{"event":"button_pressed","timestamp":12345}` | No |
| `home/esp32-01/led/state` | `{"state":"ON","timestamp":12345}` | Yes |

**Subscribed by the device:**

| Topic | Expected payload | Effect |
|---|---|---|
| `home/esp32-01/led/set` | `ON` / `OFF` | Toggles the onboard LED |
| `home/esp32-01/interval/set` | Integer (seconds), e.g. `10` | Updates temperature publish interval |

## 🛠️ Hardware

- ESP32 development board
- Onboard `BOOT` button (GPIO 0) used as the input button
- Onboard LED (`LED_BUILTIN`)

No external sensors required — temperature is read from the ESP32's internal sensor (`temperatureRead()`), purely for demonstration purposes.

## 🚀 Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- An ESP32 board
- A WiFi network with internet access
- An MQTT broker — this project defaults to the public [HiveMQ broker](https://www.hivemq.com/public-mqtt-broker/) (`broker.hivemq.com`), great for testing, not for production use

### Configuration

Open `src/main.cpp` and update the following before flashing:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_BROKER    = "broker.hivemq.com";
const char* MQTT_CLIENT_ID = "esp32-your-unique-id";
```

> ⚠️ **Note:** This is a personal/learning project. Hardcoded WiFi credentials in source are fine for a local test sketch, but should never be committed to a public repo as-is. Consider moving these to a `secrets.h` file excluded via `.gitignore` if you fork this for real use.

### Build & Upload

```bash
# Clone the repo
git clone https://github.com/<your-username>/esp32-mqtt-essentials.git
cd esp32-mqtt-essentials

# Build
pio run

# Upload to board
pio run --target upload

# Monitor serial output
pio device monitor
```

## 🧪 Testing It

You can interact with the device using any MQTT client. A couple of easy options:

**Using [MQTT Explorer](http://mqtt-explorer.com/) (GUI):**
Connect to `broker.hivemq.com:1883` and watch the `home/esp32-01/#` topic tree update live.

**Using `mosquitto_pub` / `mosquitto_sub` (CLI):**

```bash
# Watch everything the device publishes
mosquitto_sub -h broker.hivemq.com -t "home/esp32-01/#" -v

# Turn the LED on
mosquitto_pub -h broker.hivemq.com -t "home/esp32-01/led/set" -m "ON"

# Change the publish interval to 10 seconds
mosquitto_pub -h broker.hivemq.com -t "home/esp32-01/interval/set" -m "10"
```

To see the LWT in action, just unplug the board — within a few seconds the retained `status` topic will flip to `{"status":"offline"}` automatically, without the device doing anything.

## 📦 Dependencies

- [PubSubClient](https://github.com/knolleary/pubsubclient) — MQTT client library
- [ArduinoJson](https://arduinojson.org/) — JSON serialization

Both are managed automatically via `platformio.ini`.

## 📁 Project Structure

```
esp32-mqtt-essentials/
├── src/
│   └── main.cpp
├── platformio.ini
└── README.md
```

## 📄 License

This project is open source and available under the [MIT License](LICENSE).

## 🙋 About

Built as a practice project to demonstrate a solid, working understanding of MQTT on embedded hardware — covering not just "send a message" but the patterns that make MQTT-based systems reliable in practice (retained state, LWT, backoff, debouncing).
