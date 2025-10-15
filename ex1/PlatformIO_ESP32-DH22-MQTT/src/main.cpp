#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#define DHTPIN          15
#define DHTTYPE         DHT22

#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""
#define WIFI_CHANNEL    6

#define MQTT_CLIENT_ID  "wokwi"
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_TOPIC      "/g5/sensor/data"

#define SENSOR_INTERVAL 5000
#define SENSOR_LOCATION "Tehran"
#define SENSOR_ID       "Sensor1"

void callback(char* topic, byte* payload, unsigned int length);
void reconnect();

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

void setup(void) {
  Serial.begin(115200);
  dht.begin();

  Serial.println(F("DHT22 begin."));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  String payload = String(t) + "," + String(h);

  Serial.print(F("Temperature: "));
  Serial.print(t);
  Serial.print(F("°C, Humidity: "));
  Serial.print(h);
  Serial.println(F("%"));

  json j = {
    {"loc", SENSOR_LOCATION},
    {"t", t},
    {"h", h}
  };

  client.publish(MQTT_TOPIC, j.dump().c_str());
  
  delay(5000);
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = MQTT_CLIENT_ID;
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);
    }
  }
}