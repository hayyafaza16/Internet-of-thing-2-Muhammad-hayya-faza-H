#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi
const char* ssid     = "Haya";
const char* password = "1111111q";

// Konfigurasi Broker MQTT Publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort     = 1883;
const char* clientId   = "ESP8266_LabIoT_NIM104_Full";

// Pembagian Topik Mandiri
const char* topicPublish   = "kampus/labiot/nim_104/telemetri";
const char* topicSubscribe = "kampus/labiot/nim_104/kendali";
const char* topicAlert     = "kampus/labiot/nim_104/alert";
const char* topicStatus    = "kampus/labiot/nim_104/status";

// Ambang Batas Suhu Kritis (Edge Safety Engine)
const float TEMP_THRESHOLD = 31.0;

//  State Latch agar Alert tidak membanjiri broker +++
bool isOverheatAlertActive = false;

// Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)
#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000; // Siklus kirim 4 detik

void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s | Payload: %s\n", topic, incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) {
    Serial.println("Gagal membedah payload JSON!");
    return;
  }

  // Menerima parameter "led" maupun "status_led"
  int val = -1;
  if (doc.containsKey("led")) {
    val = doc["led"];
  } else if (doc.containsKey("status_led")) {
    val = doc["status_led"];
  }

  if (val == 1 || val == 0) {
    digitalWrite(ledPin, val == 1 ? HIGH : LOW);
    Serial.printf("--> [INDIKATOR] Status LED diubah: %s\n", val == 1 ? "MENYALA" : "PADAM");
  }
  Serial.println("----------------------------------------");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan kembali ke Broker...");

    // Payload Last Will and Testament (LWT) +++
    const char* lwtPayload = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

    // Connect dengan 7 parameter: ClientID, User, Pass, WillTopic, WillQoS(1), WillRetain(true), WillMessage
    if (client.connect(clientId, NULL, NULL, topicStatus, 1, true, lwtPayload)) {
      Serial.println(" Sukses Terhubung!");

      // Publikasi pesan ONLINE (Retain: true) saat berhasil terhubung +++
      const char* onlinePayload = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";
      client.publish(topicStatus, onlinePayload, true);
      Serial.printf("[STATUS LWT] Status ONLINE dikirim ke: %s\n", topicStatus);

      client.subscribe(topicSubscribe);
      Serial.printf("Subskripsi Topik: %s\n", topicSubscribe);
    } else {
      Serial.printf("Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Memelihara sesi TCP dan memproses callback masuk

  unsigned long now = millis();
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {

      // LOGIKA CLOSED-LOOP SAFETY & DEDICATED ALERT TOPIC +++
      if (t > TEMP_THRESHOLD) {
        // Otomatis nyalakan LED saat bahaya overheat
        digitalWrite(ledPin, HIGH);

        // Kirim alert hanya saat pertama kali bahaya terdeteksi (State Latching)
        if (!isOverheatAlertActive) {
          isOverheatAlertActive = true;

          StaticJsonDocument<256> docAlert;
          docAlert["status"]     = "BAHAYA";
          docAlert["parameter"]  = "OVERHEAT";
          docAlert["nilai_suhu"] = serialized(String(t, 1));

          char alertBuffer[256];
          serializeJson(docAlert, alertBuffer);
          client.publish(topicAlert, alertBuffer);
          Serial.printf("\n[ALERT BAHAYA TERKIRIM] %s\n\n", alertBuffer);
        }
      } else {
        // Reset latch jika suhu kembali normal
        isOverheatAlertActive = false;
      }

      // TELEMETRI RUTIN DENGAN KONDISI SUHU +++
      StaticJsonDocument<256> docOut;
      docOut["node_id"]      = clientId;
      docOut["suhu"]         = serialized(String(t, 2));
      docOut["kelembapan"]   = serialized(String(h, 2));
      
      // Penambahan field "kondisi_suhu"
      docOut["kondisi_suhu"] = (t > TEMP_THRESHOLD) ? "PANAS" : "NORMAL";
      
      docOut["status_led"]   = digitalRead(ledPin);

      char buffer[256];
      serializeJson(docOut, buffer);
      
      bool success = client.publish(topicPublish, buffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", buffer);
      } else {
        Serial.println("[ERROR] Gagal memublikasikan telemetri!");
      }
    }
  }
}