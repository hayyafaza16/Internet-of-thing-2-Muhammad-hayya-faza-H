#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "FERDI KOST -EXT";
const char* password = "modalngapa1";

// Konfigurasi Pin
const byte dhtPin = 2;        // D4 (GPIO 2)
const byte buttonPin = 4;     // D2 (GPIO 4)
const byte ledPin = 12;       // D6 (GPIO 12)
const byte pwmLedPin = 5;     // D1 (GPIO 5)

DHT dht(dhtPin, DHT22);

bool ledState = false;
int pwmValue = 0;             // 0 - 1023
String currentTemp = "--";
String currentHum = "--";
int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Real-Time IoT Web</title>
  <style>
    body { font-family: Arial; text-align: center; background-color: #f4f4f9; color: #333; }
    .card { background: white; margin: 15px auto; padding: 20px; max-width: 320px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    button { padding: 12px 25px; font-size: 18px; border-radius: 6px; border: none; cursor: pointer; color: white; transition: 0.3s; }
    .btn-on { background-color: #4CAF50; }
    .btn-off { background-color: #f44336; }
    input[type=range] { width: 80%; cursor: pointer; }
  </style>
</head>
<body>
  <h1>Smart Room Control</h1>
  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> &deg;C</h2>
  </div>
  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>
  <div class="card">
    <h2>LED Sakelar: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>
  <div class="card">
    <h2>Dimmer LED</h2>
    <input type="range" min="0" max="1023" value="0" id="pwmSlider"
           oninput="sendPWM(this.value)"
           onpointerdown="isDragging=true"
           onpointerup="pwmRelease(this.value)"
           onchange="pwmRelease(this.value)">
    <p>Brightness: <span id="pwmValue">0</span> / 1023</p>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    var isDragging = false;
    var lastSend = 0;

    window.addEventListener('load', initWebSocket);

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen    = function() { console.log('WebSocket Terkoneksi'); };
      websocket.onclose   = function() { setTimeout(initWebSocket, 2000); };
      websocket.onmessage = onMessage;
    }

    function safeSend(msg) {
      if (websocket && websocket.readyState === 1) websocket.send(msg);
    }

    function toggleLed() { safeSend('toggle'); }

    function sendPWM(value) {
      document.getElementById("pwmValue").innerHTML = value;
      var now = Date.now();
      if (now - lastSend > 50) {
        safeSend("pwm," + value);
        lastSend = now;
      }
    }

    function pwmRelease(value) {
      isDragging = false;
      safeSend("pwm," + value);   // pastikan nilai akhir terkirim
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);

      if (dataObj.suhu !== undefined) document.getElementById('tempValue').innerHTML = dataObj.suhu;
      if (dataObj.hum !== undefined)  document.getElementById('humValue').innerHTML = dataObj.hum;

      if (dataObj.led !== undefined) {
        var btn = document.getElementById('toggleBtn');
        var status = document.getElementById('ledStatus');
        if (dataObj.led == "1") {
          status.innerHTML = "ON";
          btn.innerHTML = "Turn OFF";
          btn.className = "btn-on";
        } else {
          status.innerHTML = "OFF";
          btn.innerHTML = "Turn ON";
          btn.className = "btn-off";
        }
      }

      if (dataObj.pwm !== undefined && !isDragging) {
        document.getElementById('pwmSlider').value = dataObj.pwm;
        document.getElementById('pwmValue').innerHTML = dataObj.pwm;
      }
    }
  </script>
</body>
</html>
)rawliteral";

void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\", ";
  jsonString += "\"pwm\":\"" + String(pwmValue) + "\", ";
  jsonString += "\"suhu\":\"" + currentTemp + "\", ";
  jsonString += "\"hum\":\"" + currentHum + "\"}";
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = String((char*)data);
    Serial.println("Pesan masuk: " + message);

    if (message == "toggle") {
      ledState = !ledState;
      notifyClients();
    }

    if (message.startsWith("pwm,")) {
      pwmValue = constrain(message.substring(4).toInt(), 0, 1023);
      analogWrite(pwmLedPin, pwmValue);
      Serial.println("PWM: " + String(pwmValue));
      // Tidak broadcast di sini agar tidak membanjiri antrean WebSocket;
      // status tersinkron lewat notifyClients() berkala di loop().
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u terhubung\n", client->id());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u terputus\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(pwmLedPin, OUTPUT);
  analogWriteRange(1023); 
  analogWriteFreq(1000);
  analogWrite(pwmLedPin, 0);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  digitalWrite(ledPin, ledState ? HIGH : LOW);

  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients();
      }
    }
  }
  lastButtonState = reading;

  if ((millis() - lastTime) > 3000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) currentTemp = String(t, 1);
    if (!isnan(h)) currentHum = String(h, 1);

    notifyClients();
    lastTime = millis();
  }
}
