#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi configuration b
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// KiwisIoT MQTT configuration
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = 1883;
const char* mqtt_user = "YOUR_USERNAME";
const char* mqtt_password = "YOUR_PASSWORD";

// MQTT topics
const char* tempTopic = "aqua/temp";
const char* phTopic = "aqua/ph";
const char* levelTopic = "aqua/level";
const char* pumpTopic = "aqua/pump";

// ESP32 pins
#define TEMP_PIN 4
#define PH_PIN 34
#define TRIG_PIN 5
#define ECHO_PIN 18
#define RELAY_PIN 26

#define PUMP_ON LOW
#define PUMP_OFF HIGH

// Tank configuration
float tankHeight = 30.0;
float minLevel = 25.0;

// pH calibration values
float voltagePH7 = 2.50;
float voltagePH4 = 3.00;

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

WiFiClient espClient;
PubSubClient client(espClient);

bool pumpStatus = false;

// Connect to WiFi
void connectWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi Connected!");
}

// Connect to MQTT
void connectMQTT() {
  while (!client.connected()) {
    if (client.connect("ESP32_Aquaponics",
                       mqtt_user,
                       mqtt_password)) {
      Serial.println("MQTT Connected!");
    } else {
      Serial.println("MQTT Connection Failed!");
      delay(3000);
    }
  }
}

// Read temperature
float getTemperature() {
  tempSensor.requestTemperatures();
  return tempSensor.getTempCByIndex(0);
}

// Read pH
float getPH() {
  int raw = analogRead(PH_PIN);
  float voltage = raw * 3.3 / 4095.0;

  float slope =
      (7.0 - 4.0) / (voltagePH7 - voltagePH4);

  return 7.0 + (voltage - voltagePH7) * slope;
}

// Read water level
float getWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return -1;

  float distance = duration * 0.0343 / 2.0;

  float level =
      ((tankHeight - distance) / tankHeight) * 100.0;

  return constrain(level, 0, 100);
}

// Pump control
void controlPump(float level) {
  if (level < 0 || level < minLevel) {
    pumpStatus = false;
  } else {
    pumpStatus = true;
  }

  digitalWrite(
      RELAY_PIN,
      pumpStatus ? PUMP_ON : PUMP_OFF
  );
}

// Send data to KiwisIoT
void sendData(const char* topic, float value) {
  char message[20];

  snprintf(message, sizeof(message), "%.2f", value);

  client.publish(topic, message);
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, PUMP_OFF);

  analogReadResolution(12);

  tempSensor.begin();

  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);
}

// Main loop
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  float temperature = getTemperature();
  float ph = getPH();
  float waterLevel = getWaterLevel();

  controlPump(waterLevel);

  Serial.println("AQUAPONICS MONITORING");

  Serial.print("Temperature: ");
  Serial.println(temperature);

  Serial.print("pH: ");
  Serial.println(ph);

  Serial.print("Water Level: ");
  Serial.println(waterLevel);

  Serial.print("Pump Status: ");
  Serial.println(pumpStatus ? "ON" : "OFF");

  if (temperature != DEVICE_DISCONNECTED_C) {
    sendData(tempTopic, temperature);
  }

  if (ph >= 0 && ph <= 14) {
    sendData(phTopic, ph);
  }

  if (waterLevel >= 0) {
    sendData(levelTopic, waterLevel);
  }

  sendData(pumpTopic, pumpStatus ? 1 : 0);

  delay(5000);
}