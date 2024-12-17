#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include "time.h"
#include "esp_sntp.h"

const char* WIFI_SSID = "WiFi ssid/name"; 
const char* WIFI_PASSWORD = "Wifi password"; 

#define API_KEY "" 
#define DATABASE_URL "" 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

int FlameSensor = 23; //D19
int Pump = 18;

String currentHistoryKey = ""; // Menyimpan key node History aktif
bool fireDetected = false; // Status deteksi api

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to retrieve date and time...");
    return "Invalid Time";
  }
  
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo); // Format: "YYYY-MM-DD HH:MM:SS"
  return String(timeStr);
}

void setup() {
  Serial.begin(9600);

  pinMode(FlameSensor, INPUT_PULLUP);
  pinMode(Pump, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign-up succeeded.");
  } else {
    Serial.printf("Sign-up error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  Serial.println("Waktu lokal:");
  Serial.println(getFormattedTime());
}

void loop() {
  String currentTime = getFormattedTime();
  int sensorState = digitalRead(FlameSensor);
  int pumpStatus = fireDetected ? HIGH : LOW;

  if (Firebase.RTDB.setString(&fbdo, "/Main/currentTime", currentTime) &&
      Firebase.RTDB.setInt(&fbdo, "/Main/sensorStatus", sensorState) &&
      Firebase.RTDB.setInt(&fbdo, "/Main/pumpStatus", pumpStatus)) {
    Serial.println("Tree Main updated.");
  } else {
    Serial.printf("Failed to update Main: %s\n", fbdo.errorReason().c_str());
  }

  if (sensorState == LOW && !fireDetected) {
    fireDetected = true;
    // digitalWrite(Pump, HIGH); // Aktifkan pompa

    FirebaseJson json;
    json.set("currentTimeStart", currentTime);
    json.set("sensorStatus", sensorState);
    json.set("pumpStatus", pumpStatus);

    if (Firebase.RTDB.pushJSON(&fbdo, "/History", &json)) {
      currentHistoryKey = fbdo.pushName(); // Simpan key node baru
      Serial.println("New History node created: " + currentHistoryKey);
    } else {
      Serial.printf("Failed to create History node: %s\n", fbdo.errorReason().c_str());
    }
  } else if (sensorState == HIGH && fireDetected) {
    fireDetected = false;

    if (Firebase.RTDB.setString(&fbdo, "/History/" + currentHistoryKey + "/currentTimeEnd", currentTime)) {
      Serial.println("History node updated with currentTimeEnd.");
    } else {
      Serial.printf("Failed to update History node: %s\n", fbdo.errorReason().c_str());
    }
    currentHistoryKey = ""; // Reset key history
  }

  delay(1000); // Update setiap detik
}
