// =============================================================================
// SMART GREENHOUSE IoT
// =============================================================================
// Board    : ESP32-S3-N16R8 Soldered 
// Firmware : All Phases (Local + WiFi + Supabase + Open-Meteo)
// Author   : 13525004@std.stei.itb.ac.id
// =============================================================================
//
// FITUR:
//   - Baca sensor: suhu (DHT11), cahaya (BH1750), hujan (Rain Sensor)
//   - Kontrol atap otomatis via servo MG996R
//   - 4 aturan kritis: Local is King, Hysteresis, Failsafe Offline, Anti-Oven
//   - Kirim data sensor ke Supabase
//   - Terima perintah manual dari Supabase
//   - Prediksi cuaca dari Open-Meteo API
//
// ARDUINO IDE SETTINGS:
//   Board          : "ESP32S3 Dev Module"
//   PSRAM          : "OPI PSRAM"
//   Flash Size     : "16MB (128Mb)"
//   USB CDC On Boot: "Enabled"
//   Upload Speed   : 921600
//
// LIBRARY YANG DIBUTUHKAN (install via Library Manager):
//   - BH1750 by Christopher Laws
//   - DHT sensor library by Adafruit
//   - Adafruit Unified Sensor by Adafruit
//   - ESP32Servo by Kevin Harrington
//   - ArduinoJson by Benoit Blanchon
//
// SETUP PERTAMA:
//   1. Copy credentials.h.example menjadi credentials.h
//   2. Isi WIFI_SSID, WIFI_PASSWORD, SUPABASE_URL, SUPABASE_KEY
//   3. Buat tabel di Supabase (lihat README.md bagian Database Schema)
//
// =============================================================================

#include "config.h"
#include "sensors.h"
#include "servo_control.h"
#include "fan_control.h"
#include "decision.h"
#include "wifi_manager.h"
#include "supabase_client.h"
#include "weather_client.h"

// Timer untuk scheduling task (semua pakai millis, TIDAK ADA delay)
unsigned long lastSensorRead = 0;
unsigned long lastSupabasePost = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastWeatherFetch = 0;

// Status sistem terakhir (untuk dikirim ke Supabase)
SystemStatus lastStatus;

void setup() {
  // Mulai serial untuk debugging via Serial Monitor
  Serial.begin(115200);

  // Tunggu serial monitor siap (khusus ESP32-S3 dengan USB native)
  // Tanpa ini, pesan awal bisa hilang karena USB CDC belum ready
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("============================================================");
  Serial.println("  SMART GREENHOUSE IoT");
  Serial.println("  Board: ESP32-S3-N16R8 Soldered");
  Serial.println("  Firmware: All Phases Active");
  Serial.println("============================================================");
  Serial.printf("  Free heap  : %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  Free PSRAM : %d bytes\n", ESP.getFreePsram());
  Serial.println("============================================================");
  Serial.println();

  // Inisialisasi komponen
  sensorsInit();
  servoInit();
  fanInit();
  decisionInit();

  wifiInit();
  decisionSetWifiStatus(wifiIsConnected());

  if (wifiIsConnected()) {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
    Serial.println("[SETUP] Sinkron jam NTP...");
    delay(1500);

    Serial.println("[SETUP] Ambil prediksi cuaca awal...");
    WeatherForecast forecast = weatherFetch();
    if (forecast.fetchSuccess) {
      decisionSetForecast(forecast.willRain, forecast.rainProbability);
      weatherUpload(forecast);
    }
    lastWeatherFetch = millis();
  }

  Serial.println();
  Serial.println("[SYSTEM] Inisialisasi selesai. Memulai loop utama...");
  Serial.println();
}

void loop() {
  unsigned long now = millis();

  // =========================================================================
  // WiFi: Non-blocking reconnect kalau putus (Rule 3: Failsafe Offline)
  // =========================================================================
  wifiReconnectIfNeeded();
  decisionSetWifiStatus(wifiIsConnected());

  // =========================================================================
  // SENSOR READ — Tiap 5 detik
  // =========================================================================
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;

    // Baca semua sensor
    SensorData sensorData = sensorsRead();

    // Proses keputusan (otak sistem)
    lastStatus = decisionProcess(sensorData);

    // Print status ke Serial Monitor
    Serial.println("------------------------------------------------------------");
    Serial.printf("  Suhu       : %.1f C\n", lastStatus.sensors.temperature);
    Serial.printf("  Humidity   : %.1f %%\n", lastStatus.sensors.humidity);
    Serial.printf("  Cahaya     : %.0f lux\n", lastStatus.sensors.lux);
    Serial.printf("  Hujan      : %s\n", lastStatus.sensors.isRaining ? "YA" : "TIDAK");
    Serial.printf("  Atap       : %s (%d derajat)\n",
      (lastStatus.roofState == ROOF_OPEN) ? "BUKA" :
      (lastStatus.roofState == ROOF_CLOSED) ? "TUTUP" : "TRANSISI",
      lastStatus.roofAngle);
    Serial.printf("  Mode       : %s\n",
      (lastStatus.mode == MODE_AUTO) ? "AUTO" : "MANUAL");
    Serial.printf("  Overheat   : %s\n",
      lastStatus.overheating ? "WARNING!" : "Normal");
    Serial.printf("  Kipas      : %s (%s)\n",
      lastStatus.fanOn ? "NYALA" : "MATI",
      (lastStatus.fanMode == FAN_AUTO) ? "AUTO" : "MANUAL");
    Serial.printf("  WiFi       : %s\n",
      lastStatus.wifiConnected ? "Terhubung" : "Offline");
    Serial.printf("  Forecast   : hujan=%s (%d%%)\n",
      lastStatus.forecastRain ? "YA" : "TIDAK",
      lastStatus.forecastRainProbability);
    Serial.printf("  Free heap  : %d bytes\n", ESP.getFreeHeap());
    Serial.println();
  }

  // =========================================================================
  // SUPABASE POST — Kirim data sensor tiap 60 detik (kalau online)
  // =========================================================================
  if (wifiIsConnected() && (now - lastSupabasePost >= SUPABASE_POST_INTERVAL)) {
    lastSupabasePost = now;
    supabasePostSensorData(lastStatus);
  }

  // =========================================================================
  // SUPABASE CHECK — Cek perintah manual tiap 10 detik (kalau online)
  // =========================================================================
  if (wifiIsConnected() && (now - lastCommandCheck >= SUPABASE_CHECK_INTERVAL)) {
    lastCommandCheck = now;

    int command = supabaseCheckCommand();

    if (command == SERVO_OPEN || command == SERVO_CLOSED) {
      SensorData freshData = sensorsRead();
      bool accepted = decisionManualOverride(command, freshData);
      if (accepted) {
        Serial.printf("[MAIN] Perintah atap DITERIMA: %s\n",
          command == SERVO_OPEN ? "BUKA" : "TUTUP");
      } else {
        Serial.println("[MAIN] Perintah atap DITOLAK (hujan terdeteksi)");
      }
    } else if (command == CMD_AUTO) {
      decisionSetMode(MODE_AUTO);
      Serial.println("[MAIN] Atap kembali ke mode AUTO");
    } else if (command == CMD_FAN_ON) {
      fanSetManual(true);
    } else if (command == CMD_FAN_OFF) {
      fanSetManual(false);
    } else if (command == CMD_FAN_AUTO) {
      fanSetMode(FAN_AUTO);
    }
  }

  // =========================================================================
  // WEATHER FETCH — Ambil prediksi cuaca tiap 30 menit (kalau online)
  // =========================================================================
  if (wifiIsConnected() && (now - lastWeatherFetch >= WEATHER_FETCH_INTERVAL)) {
    lastWeatherFetch = now;

    WeatherForecast forecast = weatherFetch();
    if (forecast.fetchSuccess) {
      decisionSetForecast(forecast.willRain, forecast.rainProbability);
      weatherUpload(forecast);
    }
  }

  // =========================================================================
  // SERVO UPDATE — Panggil setiap loop untuk cek hysteresis timer
  // Kalau atap baru gerak ATAU kipas baru berubah → kirim sensor_log instan
  // (biar dashboard cepat tau, tanpa nunggu interval 60 detik berikutnya)
  // =========================================================================
  bool roofMoved = servoUpdate();
  bool fanChanged = fanStateChangedSinceLastPost();

  if (wifiIsConnected() && (roofMoved || fanChanged)) {
    lastStatus.roofState = servoGetState();
    lastStatus.roofAngle = servoGetCurrentAngle();
    lastStatus.fanOn = fanIsOn();
    lastStatus.fanMode = fanGetMode();
    supabasePostSensorData(lastStatus);
    lastSupabasePost = now;
  }
}
