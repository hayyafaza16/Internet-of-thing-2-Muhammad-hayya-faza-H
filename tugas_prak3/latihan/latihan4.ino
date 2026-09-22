#include <ESP8266WiFi.h>  
#include <ESP8266HTTPClient.h>  
#include <WiFiClient.h>
#include <DHT.h>

const char* ssid = "XP";  
const char* password = "1a2b3C4D";

const char* serverName = "http://10.36.43.193/relay/on";   
const byte dhtPin = 2; // GPIO2 / D4
DHT dhtClient(dhtPin, DHT22); // Sesuaikan DHT22

void setup() {  
  Serial.begin(115200);
  dhtClient.begin();
  WiFi.mode(WIFI_STA);  
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }  
  Serial.println("\nClient Terhubung ke Wi-Fi!");  
}

void loop() {  
  float temp = dhtClient.readTemperature();

  if (isnan(temp)) {
    Serial.println("Gagal membaca dari sensor DHT!");
    delay(2000);
    return;
  }

  Serial.print("Suhu Terbaca: ");
  Serial.print(temp);
  Serial.println(" °C");
    
  if ((WiFi.status() == WL_CONNECTED) && (temp > 35.0)) {  
    WiFiClient client;  
    HTTPClient http;  
      
    http.begin(client, serverName);  
    int httpResponseCode = http.GET();  
      
    Serial.print("HTTP Response code: ");  
    Serial.println(httpResponseCode); 
      
    http.end();  
    delay(10000); 
  }  
  delay(2000);  
}