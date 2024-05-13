#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <PMS.h>
#include <ArduinoJson.h>

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* sensor_topic = "sensors/data";

#define DHT_PIN 26  // GPIO pin connected to the DHT sensor
#define DHT_TYPE DHT22
#define PMS_RX_PIN 22 // PMS5003 RX pin connected to ESP32 TX pin
#define PMS_TX_PIN 21 // PMS5003 TX pin connected to ESP32 RX pin

DHT dht(DHT_PIN, DHT_TYPE);
PMS pms(Serial1); // Initialize PMS5003 sensor with Serial1
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming MQTT messages if needed
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(sensor_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Serial1.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN); // Initialize Serial1 for PMS5003
  setup_wifi();
  client.setServer(mqtt_server, 1885);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(10000); // Adjust delay according to your preference

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  PMS::DATA data;

  // Read sensor data from PMS5003
  pms.wakeUp();
  delay(30000); // Delay for sensor stabilization
  pms.requestRead();
  if (pms.readUntil(data)) {
    pms.sleep(); // Put the PMS5003 sensor to sleep
  } else {
    Serial.println("Failed to read data from PMS5003 sensor");
    pms.sleep(); // Ensure the PMS5003 sensor is put to sleep even if failed to read
  }

  // Check if DHT sensor is connected
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read data from DHT sensor");
    return;
  }

  // Check if PMS5003 sensor is connected
  if (data.PM_AE_UG_2_5 == 0 && data.PM_AE_UG_10_0 == 0) {
    Serial.println("Failed to read data from PMS5003 sensor");
    return;
  }

  // Create a JSON object
  StaticJsonDocument<200> jsonDoc;
  
  // Add temperature and humidity data from DHT sensor
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  
  // Add PM2.5 and PM10 data from PMS5003 sensor
  jsonDoc["pm25"] = data.PM_AE_UG_2_5;
  jsonDoc["pm10"] = data.PM_AE_UG_10_0;

  // Serialize the JSON object to a string
  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);
  
  // Publish the JSON string to the MQTT topic
  client.publish(sensor_topic, jsonBuffer);
}
