#include "weather_client.h"
#include "config.h"
#include "credentials.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

WeatherForecast weatherFetch() {
  WeatherForecast f;
  f.fetchSuccess = false;
  f.willRain = false;
  f.rainProbability = 0;
  f.forecastTemp = 0;
  f.humidity = 0;
  f.weatherCode = 0;
  f.hour = 0;

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast"
    "?latitude=" LATITUDE
    "&longitude=" LONGITUDE
    "&hourly=temperature_2m,precipitation_probability,relative_humidity_2m,weathercode"
    "&forecast_days=1"
    "&timezone=Asia/Jakarta";

  http.begin(url);
  http.setTimeout(WEATHER_TIMEOUT);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[WEATHER] Gagal fetch, HTTP %d\n", code);
    http.end();
    return f;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    Serial.println("[WEATHER] JSON parse error");
    return f;
  }

  JsonArray probs = doc["hourly"]["precipitation_probability"];
  JsonArray temps = doc["hourly"]["temperature_2m"];
  JsonArray hums  = doc["hourly"]["relative_humidity_2m"];
  JsonArray codes = doc["hourly"]["weathercode"];
  if (probs.isNull() || temps.isNull()) return f;

  time_t nowTs = time(nullptr);
  struct tm tinfo;
  localtime_r(&nowTs, &tinfo);
  int idx = tinfo.tm_hour;
  if (idx >= (int)probs.size()) idx = probs.size() - 1;
  if (idx < 0) idx = 0;

  f.rainProbability = probs[idx].as<int>();
  f.forecastTemp = temps[idx].as<float>();
  f.humidity = hums.isNull() ? 0 : hums[idx].as<float>();
  f.weatherCode = codes.isNull() ? 0 : codes[idx].as<int>();
  f.willRain = (f.rainProbability >= RAIN_PROBABILITY_THRESHOLD);
  f.hour = idx;
  f.fetchSuccess = true;

  Serial.printf("[WEATHER] Jam %02d:00 - hujan %d%%, suhu %.1f C, lembap %.0f%%, code %d\n",
    f.hour, f.rainProbability, f.forecastTemp, f.humidity, f.weatherCode);

  return f;
}

bool weatherUpload(WeatherForecast f) {
  if (!f.fetchSuccess) return false;

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/weather?id=eq.1";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(HTTP_TIMEOUT);

  char hourLabel[6];
  snprintf(hourLabel, sizeof(hourLabel), "%02d:00", f.hour);

  JsonDocument doc;
  doc["rain_probability"] = f.rainProbability;
  doc["temperature"] = f.forecastTemp;
  doc["humidity"] = f.humidity;
  doc["weather_code"] = f.weatherCode;
  doc["hour_label"] = hourLabel;

  String payload;
  serializeJson(doc, payload);

  int code = http.PATCH(payload);
  http.end();

  if (code == 200 || code == 204) {
    Serial.println("[WEATHER] Cuaca dikirim ke Supabase");
    return true;
  }
  Serial.printf("[WEATHER] Gagal kirim ke Supabase, HTTP %d\n", code);
  return false;
}
