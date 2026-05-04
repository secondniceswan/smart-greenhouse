#include "wifi_manager.h"
#include "config.h"
#include "credentials.h"
#include <WiFi.h>

// Waktu terakhir mencoba reconnect
unsigned long lastReconnectAttempt = 0;

void wifiInit() {
  // Set mode Station (client, bukan Access Point)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WIFI] Menghubungkan ke ");
  Serial.print(WIFI_SSID);

  // Tunggu max 10 detik untuk koneksi pertama kali
  // Ini satu-satunya tempat kita boleh "menunggu" — karena di setup(), bukan loop()
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Terhubung! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal strength: %d dBm\n", WiFi.RSSI());
  } else {
    // Gagal connect bukan masalah fatal
    // Sistem tetap jalan dengan sensor lokal (Rule 3: Failsafe Offline)
    Serial.println("[WIFI] Gagal terhubung — lanjut mode offline");
    Serial.println("[WIFI] Sensor dan servo tetap aktif secara lokal");
  }
}

bool wifiIsConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

void wifiReconnectIfNeeded() {
  // Kalau sudah connected, tidak perlu apa-apa
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  // Coba reconnect setiap WIFI_RECONNECT_INTERVAL (30 detik)
  // Ini NON-BLOCKING — WiFi.begin() return langsung, tidak menunggu
  // Koneksi akan establish di background oleh WiFi stack ESP32
  unsigned long now = millis();
  if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
    lastReconnectAttempt = now;
    Serial.println("[WIFI] Mencoba reconnect...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // Tidak ada delay/wait di sini — loop() tetap berjalan normal
    // Kalau berhasil connect, WiFi.status() akan berubah sendiri
  }
}
