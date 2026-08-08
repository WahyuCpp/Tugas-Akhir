/*
 * ESP32 induction motor vibration monitor
 * Publishes batched ADXL335 samples to AWS IoT Core over MQTT/TLS (mTLS).
 * FFT is NOT computed here — raw batches only, processed server-side in Lambda.
 *
 * Libraries required (Arduino Library Manager):
 *   - PubSubClient (Nick O'Leary)
 *   - ArduinoJson
 *   WiFiClientSecure and WiFi are bundled with the ESP32 board package.
 *
 * Before uploading:
 *   1. Copy secrets.h.example -> secrets.h and fill in your values
 *   2. Confirm SAMPLE_RATE_HZ here matches SampleRateHz in infrastructure/template.yaml
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

// ---- Configuration ----
#define ADXL335_X_PIN 34
#define ADXL335_Y_PIN 35
#define ADXL335_Z_PIN 32

const uint32_t SAMPLE_RATE_HZ = 1000;      // must match Lambda SAMPLE_RATE_HZ
// JSON-encodes to ~9 bytes/float * 3 axes -> at 1kHz, 1s of samples is
// ~27KB, over the MQTT payload budget below. Batch in smaller windows
// (published more frequently) instead of one large payload per FFT window;
// the Lambda ingest handler reassembles them from the rolling buffer.
// For higher sustained sample rates, switch to CBOR/MessagePack encoding
// or increase BATCH_DURATION_MS trade-off deliberately after measuring
// actual payload size against the 128KB MQTT hard cap.
const uint32_t BATCH_DURATION_MS = 200;    // publish 5x/second
const uint32_t N_SAMPLES = (SAMPLE_RATE_HZ * BATCH_DURATION_MS) / 1000;

char mqttTopic[64];
WiFiClientSecure net;
PubSubClient client(net);

float bufX[N_SAMPLES], bufY[N_SAMPLES], bufZ[N_SAMPLES];

// ---- Wi-Fi ----
void connectWiFi() {
  Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected: " + WiFi.localIP().toString());
}

// ---- NTP time sync — REQUIRED before TLS handshake or cert validity check fails ----
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {  // wait until time is plausible (post-1970 + margin)
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nTime synced: " + String(now));
}

// ---- MQTT / TLS ----
void connectAWS() {
  net.setCACert(AWS_ROOT_CA1);
  net.setCertificate(DEVICE_CERT);
  net.setPrivateKey(DEVICE_PRIVATE_KEY);

  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setBufferSize(6144);  // default 256B is too small for a sample batch

  Serial.print("Connecting to AWS IoT Core");
  while (!client.connected()) {
    if (client.connect(THING_NAME)) {
      Serial.println("\nConnected to AWS IoT Core");
    } else {
      Serial.printf(" failed, rc=%d, retrying in 2s\n", client.state());
      delay(2000);
    }
  }
}

// ---- Sampling ----
// NOTE: ADXL335 output is analog voltage proportional to acceleration.
// Convert raw ADC counts to physical units (g) based on your board's
// reference voltage and the ADXL335 sensitivity (~300 mV/g typical).
// Calibrate ZERO_G_VOLTAGE and SENSITIVITY_V_PER_G for your specific unit.
const float ADC_VREF = 3.3;
const float ADC_MAX = 4095.0;
const float ZERO_G_VOLTAGE = 1.65;
const float SENSITIVITY_V_PER_G = 0.330;

float adcToG(int raw) {
  float voltage = (raw / ADC_MAX) * ADC_VREF;
  return (voltage - ZERO_G_VOLTAGE) / SENSITIVITY_V_PER_G;
}

void sampleBatch() {
  uint32_t intervalUs = 1000000UL / SAMPLE_RATE_HZ;
  for (uint32_t i = 0; i < N_SAMPLES; i++) {
    uint32_t t0 = micros();
    bufX[i] = adcToG(analogRead(ADXL335_X_PIN));
    bufY[i] = adcToG(analogRead(ADXL335_Y_PIN));
    bufZ[i] = adcToG(analogRead(ADXL335_Z_PIN));
    while (micros() - t0 < intervalUs) { /* busy-wait to hold sample rate */ }
  }
}

// ---- Publish ----
void publishBatch() {
  StaticJsonDocument<6144> doc;
  doc["deviceTimestamp"] = millis();

  JsonArray x = doc.createNestedArray("x");
  JsonArray y = doc.createNestedArray("y");
  JsonArray z = doc.createNestedArray("z");
  for (uint32_t i = 0; i < N_SAMPLES; i++) {
    x.add(bufX[i]);
    y.add(bufY[i]);
    z.add(bufZ[i]);
  }

  char payload[6144];
  size_t len = serializeJson(doc, payload);

  if (!client.publish(mqttTopic, (const uint8_t*)payload, len, false)) {
    Serial.println("Publish failed — check buffer size / QoS / connection state");
  }
}

void setup() {
  Serial.begin(115200);
  snprintf(mqttTopic, sizeof(mqttTopic), "sensors/raw/%s", THING_NAME);

  connectWiFi();
  syncTime();
  connectAWS();
}

void loop() {
  if (!client.connected()) {
    connectAWS();
  }
  client.loop();  // MUST be called frequently — services keep-alive (PINGREQ)

  sampleBatch();
  publishBatch();
}
