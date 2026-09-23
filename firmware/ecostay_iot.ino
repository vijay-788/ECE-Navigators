/*
  EcoStay IoT — Intelligent Edge-Driven Energy Optimizer for Budget Hotels
  Team: ECE Navigators | Smart India Hackathon 2026 | PS ID: 26221

  Hardware:
    - ESP32 DevKit V1 (central processing unit)
    - AMG8833 8x8 Thermal Array Sensor (I2C) — occupancy detection via body heat
    - RC522 RFID Module (SPI) — guest check-in / door access
    - DHT22 Temperature & Humidity Sensor (1-Wire) — ambient calibration
    - Dual 30A Optocoupler-Isolated Relay Module — AC & Geyser load control

  Logic:
    RFID swipe -> energize room -> AMG8833 continuously scans for a human
    thermal signature relative to the DHT22 ambient baseline -> if the room
    reads zero occupants for a continuous 5-minute window, relays cut power
    to the AC/Geyser (heavy inductive loads only; low-power sockets stay live).
    Live status is streamed to a cloud dashboard over Wi-Fi/MQTT every 10s.
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_AMG88xx.h>
#include <MFRC522.h>
#include <DHT.h>

// ====================================================================
// NETWORK & MQTT CONFIGURATION
// ====================================================================
const char* ssid        = "YOUR_HOTEL_WIFI_SSID";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP_OR_HOST"; // e.g. a HiveMQ/Mosquitto broker address
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "ecostay/hotel/room101/telemetry";

// ====================================================================
// HARDWARE PIN ALLOCATIONS
// ====================================================================
#define RFID_SS_PIN      5   // SPI SDA / Slave Select
#define RFID_RST_PIN     4   // SPI Reset Pin
#define DHT_PIN          15  // 1-Wire DHT22 Data Pin
#define RELAY_AC_PIN     26  // IN1: AC Control Relay
#define RELAY_GEYSER_PIN 27  // IN2: Geyser Control Relay
#define DHTTYPE          DHT22
#define I2C_SDA_PIN      21  // AMG8833 I2C Data
#define I2C_SCL_PIN      22  // AMG8833 I2C Clock

// ====================================================================
// CALIBRATION VALUES
// ====================================================================
const int MIN_PIXEL_CLUSTER     = 3;       // warm pixels required to confirm a human signature
const unsigned long TIMEOUT_VAL = 300000;  // 5 minutes in ms

// ====================================================================
// OBJECT INITIALIZATIONS
// ====================================================================
Adafruit_AMG88xx amg;
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
DHT dht(DHT_PIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ====================================================================
// STATE TRACKERS
// ====================================================================
bool isRoomOccupiedByHeat   = false;
bool isKeycardSwipedIn      = false;
unsigned long emptyRoomTime = 0;
float thermalMatrix[AMG88XX_PIXEL_ARRAY_SIZE];
unsigned long lastTelemetryTransmission = 0;

// ====================================================================
// NETWORK UTILITIES
// ====================================================================
void setup_wifi() {
  delay(10);
  Serial.print("\n[Wi-Fi] Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int loopCount = 0;
  while (WiFi.status() != WL_CONNECTED && loopCount < 15) {
    delay(500);
    Serial.print(".");
    loopCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi] Connected! IP Address:");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[Wi-Fi] Offline — running in standalone edge mode.");
  }
}

void reconnect_mqtt() {
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    Serial.print("[MQTT] Connecting to broker...");
    String clientId = "ESP32Client-Room101-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

// ====================================================================
// SETUP
// ====================================================================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_AC_PIN, OUTPUT);
  pinMode(RELAY_GEYSER_PIN, OUTPUT);
  digitalWrite(RELAY_AC_PIN, LOW);
  digitalWrite(RELAY_GEYSER_PIN, LOW);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!amg.begin()) {
    Serial.println("[-] AMG8833 not detected. Halting.");
    while (1);
  }
  Serial.println("[+] AMG8833 Thermal Array Verified.");

  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("[+] MFRC522 RFID Initialized.");

  dht.begin();
  Serial.println("[+] DHT22 Environmental Module Armed.");

  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_port);

  Serial.println("==================================================");
  Serial.println("   ECE NAVIGATORS: ECOSTAY SYSTEM READY");
  Serial.println("==================================================\n");
}

// ====================================================================
// MAIN LOOP
// ====================================================================
void loop() {
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  mqttClient.loop();

  // ---- Step 1: RFID entry ----
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("\n[RFID] Keycard Authenticated! Activating room lines.");
    isKeycardSwipedIn = true;
    digitalWrite(RELAY_AC_PIN, HIGH);
    digitalWrite(RELAY_GEYSER_PIN, HIGH);
    mfrc522.PICC_HaltA();
  }

  // ---- Step 2: Ambient calibration ----
  float ambientTemp = dht.readTemperature();
  if (isnan(ambientTemp)) {
    ambientTemp = 25.0; // fail-safe default if the 1-Wire line drops
  }

  // ---- Step 3: Thermal occupancy scan ----
  amg.readPixels(thermalMatrix);
  int detectedHumanPixels = 0;
  float dynamicMinLimit = ambientTemp + 2.5;
  float dynamicMaxLimit = ambientTemp + 10.0;
  for (int i = 0; i < AMG88XX_PIXEL_ARRAY_SIZE; i++) {
    if (thermalMatrix[i] >= dynamicMinLimit && thermalMatrix[i] <= dynamicMaxLimit) {
      detectedHumanPixels++;
    }
  }
  isRoomOccupiedByHeat = (detectedHumanPixels >= MIN_PIXEL_CLUSTER);

  // ---- Step 4: Load-shedding decision logic ----
  if (isRoomOccupiedByHeat) {
    digitalWrite(RELAY_AC_PIN, HIGH);
    digitalWrite(RELAY_GEYSER_PIN, HIGH);
    emptyRoomTime = 0;
  } else if (digitalRead(RELAY_AC_PIN) == HIGH || digitalRead(RELAY_GEYSER_PIN) == HIGH) {
    if (emptyRoomTime == 0) {
      emptyRoomTime = millis();
      Serial.println("\n[LOGIC] No occupant detected. Starting 5-minute timeout...");
    }
    unsigned long currentDuration = millis() - emptyRoomTime;
    Serial.print("[TIMER] Cooldown active. Cutoff in: ");
    Serial.print((TIMEOUT_VAL - currentDuration) / 1000);
    Serial.println(" s");

    if (currentDuration >= TIMEOUT_VAL) {
      Serial.println("\n[ACTUATION] Timeout reached — shedding AC/Geyser load.");
      digitalWrite(RELAY_AC_PIN, LOW);
      digitalWrite(RELAY_GEYSER_PIN, LOW);
      isKeycardSwipedIn = false;
      emptyRoomTime = 0;
    }
  }

  // ---- Step 5: Telemetry to dashboard ----
  if (millis() - lastTelemetryTransmission > 10000) {
    lastTelemetryTransmission = millis();
    if (mqttClient.connected()) {
      String payload = "{";
      payload += "\"room_occupied\":" + String(isRoomOccupiedByHeat ? "true" : "false") + ",";
      payload += "\"card_present\":" + String(isKeycardSwipedIn ? "true" : "false") + ",";
      payload += "\"ambient_temp\":" + String(ambientTemp) + ",";
      payload += "\"ac_status\":" + String(digitalRead(RELAY_AC_PIN) == HIGH ? "true" : "false") + ",";
      payload += "\"geyser_status\":" + String(digitalRead(RELAY_GEYSER_PIN) == HIGH ? "true" : "false");
      payload += "}";
      mqttClient.publish(mqtt_topic, payload.c_str());
      Serial.println("[TELEMETRY] Live state pushed to dashboard.");
    }
  }

  delay(1000);
}
