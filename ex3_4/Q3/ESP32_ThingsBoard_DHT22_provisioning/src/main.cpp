#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include <ArduinoJson.h>
#include <DHT.h>

constexpr char WIFI_SSID[] = "Wokwi-GUEST";
constexpr char WIFI_PASSWORD[] = "";
constexpr char THINGSBOARD_SERVER[] = "host.wokwi.internal";
constexpr uint16_t THINGSBOARD_PORT = 1883;

constexpr char PROV_KEY[]    = "jpe1o0wus2pxfutdbcra";
constexpr char PROV_SECRET[] = "yk2hn65xf6tio0of9jwv";
constexpr char DEVICE_NAME[] = "ESP32-DHT22-001";

constexpr uint32_t CLAIMING_REQUEST_DURATION_MS = (60UL * 60UL * 1000UL);
std::string claimingRequestSecretKey = "123";

#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

char DYNAMIC_TOKEN[64] = {0};
bool isProvisioned = false;
bool claimingRequestSent = false;

WiFiClient net;
PubSubClient provisionMqtt(net);
Arduino_MQTT_Client tbMqtt(net);
ThingsBoard tb(tbMqtt, 128, 128);

volatile bool provResponseArrived = false;
StaticJsonDocument<256> provResponseDoc;

void waitForWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" connected");
}

void onProvisionMessage(char*, byte* payload, unsigned int len) {
  DeserializationError err = deserializeJson(provResponseDoc, payload, len);
  if (!err) provResponseArrived = true;
}

bool runProvisioning() {
  provisionMqtt.setServer(THINGSBOARD_SERVER, THINGSBOARD_PORT);
  provisionMqtt.setCallback(onProvisionMessage);
  if (!provisionMqtt.connect("esp32-provision", "provision", "")) {
    Serial.println("Provision MQTT connect failed");
    return false;
  }
  if (!provisionMqtt.subscribe("/provision/response")) {
    Serial.println("Subscribe to /provision/response failed");
    return false;
  }

  StaticJsonDocument<256> req;
  req["deviceName"] = DEVICE_NAME;
  req["provisionDeviceKey"] = PROV_KEY;
  req["provisionDeviceSecret"] = PROV_SECRET;
  char buf[256];
  size_t n = serializeJson(req, buf, sizeof(buf));

  if (!provisionMqtt.publish("/provision/request", buf, n)) {
    Serial.println("Publish /provision/request failed");
    return false;
  }

  uint32_t start = millis();
  while (!provResponseArrived && millis() - start < 8000) {
    provisionMqtt.loop();
    delay(10);
  }
  provisionMqtt.disconnect();

  const char* status = provResponseDoc["status"] | provResponseDoc["provisionDeviceStatus"];
  if (!status || strcmp(status, "SUCCESS") != 0) {
    Serial.print("Provision failed, status=");
    Serial.println(status ? status : "null");
    return false;
  }

  const char* type  = provResponseDoc["credentialsType"] | "";
  const char* value = provResponseDoc["credentialsValue"] | provResponseDoc["accessToken"] | "";
  if (strcmp(type, "ACCESS_TOKEN") != 0 || strlen(value) == 0) {
    Serial.println("Unexpected credentials payload");
    return false;
  }

  strncpy(DYNAMIC_TOKEN, value, sizeof(DYNAMIC_TOKEN) - 1);
  return true;
}

bool ensureTbConnected() {
  if (tb.connected()) return true;
  if (!isProvisioned) return false;
  Serial.printf("Connecting to TB %s with token %s\n", THINGSBOARD_SERVER, DYNAMIC_TOKEN);
  if (!tb.connect(THINGSBOARD_SERVER, DYNAMIC_TOKEN, THINGSBOARD_PORT)) {
    Serial.println("TB connect failed");
    return false;
  }
  Serial.println("Connected to ThingsBoard");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nBooting...");
  dht.begin();
  waitForWiFi();
  isProvisioned = runProvisioning();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) waitForWiFi();
  if (!ensureTbConnected()) { delay(1000); return; }

  if (!claimingRequestSent) {
    Serial.printf("Sending claim request (secret len=%u, duration=%lums)\n",
                  (unsigned)claimingRequestSecretKey.length(),
                  (unsigned long)CLAIMING_REQUEST_DURATION_MS);
    claimingRequestSent = tb.Claim_Request(claimingRequestSecretKey.c_str(), CLAIMING_REQUEST_DURATION_MS);
    if (claimingRequestSent) Serial.println("Claim request sent. Customer must finish claim via widget.");
    else Serial.println("Claim request failed to send.");
  }

  static uint32_t lastSend = 0;
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor");
    } else {
      Serial.printf("DHT22 -> T=%.2f °C  H=%.2f %%\n", t, h);
      tb.sendTelemetryData("temperature", t);
      tb.sendTelemetryData("humidity", h);
    }
  }
  tb.loop();
  delay(10);
}
