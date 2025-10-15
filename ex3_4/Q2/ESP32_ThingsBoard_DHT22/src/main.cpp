#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22

const char* ssid       = "Wokwi-GUEST";
const char* password   = "";
const char* mqttServer = "host.wokwi.internal";     
const int   mqttPort   = 1883;
const char* token      = "TKVSKrTgJxKFHIok9KjJ"; 

WiFiClient espClient;
PubSubClient mqtt(espClient);
DHT dht(DHTPIN, DHTTYPE);

void reconnect() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect("ESP32Client", token, nullptr)) {
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 3 seconds");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password, 6);  
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  mqtt.setServer(mqttServer, mqttPort);
}

void loop() {
  if (!mqtt.connected()) reconnect();
  mqtt.loop();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    String payload = "{\"temperature\":" + String(temperature) +
                     ",\"humidity\":" + String(humidity) + "}";
    mqtt.publish("v1/devices/me/telemetry", payload.c_str());
    Serial.println("Published: " + payload);
  } else {
    Serial.println("Failed to read from DHT");
  }

  delay(5000);
}
