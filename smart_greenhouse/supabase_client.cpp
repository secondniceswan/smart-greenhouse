#include "supabase_client.h"
#include "config.h"
#include "credentials.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool supabasePostSensorData(SystemStatus status) {
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/sensor_logs";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(HTTP_TIMEOUT);

  // Buat JSON payload dengan ArduinoJson
  JsonDocument doc;
  doc["temperature"]  = status.sensors.temperature;
  doc["humidity"]     = status.sensors.humidity;
  doc["lux"]          = status.sensors.lux;
  doc["is_raining"]   = status.sensors.isRaining;
  doc["roof_angle"]   = status.roofAngle;
  doc["roof_state"]   = (status.roofState == ROOF_OPEN) ? "open" : "closed";
  doc["mode"]         = (status.mode == MODE_AUTO) ? "auto" : "manual";
  doc["overheating"]  = status.overheating;

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  http.end();

  if (httpCode == 201) {
    Serial.println("[SUPABASE] Data sensor terkirim");
    return true;
  } else {
    // Gagal kirim bukan masalah fatal — data akan dikirim lagi di interval berikutnya
    // Kita tidak meng-buffer data lama karena RAM terbatas
    Serial.printf("[SUPABASE] Gagal kirim data, HTTP %d\n", httpCode);
    return false;
  }
}

int supabaseCheckCommand() {
  HTTPClient http;

  // Query: ambil 1 perintah terbaru yang belum dieksekusi
  // executed=eq.false → hanya yang belum dijalankan
  // order=created_at.desc → yang terbaru duluan
  // limit=1 → cuma ambil 1
  String url = String(SUPABASE_URL)
    + "/rest/v1/commands?executed=eq.false&order=created_at.desc&limit=1";

  http.begin(url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(HTTP_TIMEOUT);

  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    Serial.printf("[SUPABASE] Gagal cek perintah, HTTP %d\n", httpCode);
    return -1;
  }

  String response = http.getString();
  http.end();

  // Parse response JSON
  // Response dari Supabase REST API: array of objects → [{"id":1, "action":"open", ...}]
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.printf("[SUPABASE] JSON parse error: %s\n", error.c_str());
    return -1;
  }

  // Cek apakah array kosong (tidak ada perintah baru)
  if (doc.size() == 0) {
    return -1;
  }

  // Ambil data dari perintah pertama (terbaru)
  String action = doc[0]["action"].as<String>();
  int commandId = doc[0]["id"].as<int>();

  Serial.printf("[SUPABASE] Perintah diterima: action=%s, id=%d\n",
    action.c_str(), commandId);

  // Tandai perintah sudah dieksekusi supaya tidak diproses ulang
  supabaseMarkCommandDone(commandId);

  // Parse action
  if (action == "open") return SERVO_OPEN;      // 180
  if (action == "close") return SERVO_CLOSED;    // 0
  if (action == "auto") return -2;               // Kembali ke mode auto

  Serial.printf("[SUPABASE] Action tidak dikenal: %s\n", action.c_str());
  return -1;
}

bool supabaseMarkCommandDone(int commandId) {
  HTTPClient http;

  // PATCH: update field "executed" jadi true untuk command dengan id tertentu
  String url = String(SUPABASE_URL)
    + "/rest/v1/commands?id=eq." + String(commandId);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(HTTP_TIMEOUT);

  int httpCode = http.PATCH("{\"executed\":true}");
  http.end();

  if (httpCode == 200 || httpCode == 204) {
    Serial.printf("[SUPABASE] Perintah #%d ditandai selesai\n", commandId);
    return true;
  } else {
    Serial.printf("[SUPABASE] Gagal tandai perintah #%d, HTTP %d\n", commandId, httpCode);
    return false;
  }
}
