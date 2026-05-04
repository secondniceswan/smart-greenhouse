#include "sensors.h"
#include "config.h"
#include <Wire.h>

// Object sensor global (dipakai internal di file ini saja)
BH1750 lightMeter;
DHT dht(PIN_DHT11, DHT11);

void sensorsInit() {
  // Inisialisasi I2C untuk BH1750
  // ESP32-S3: WAJIB specify pin SDA dan SCL secara eksplisit
  // karena default I2C S3 (GPIO8/GPIO9) berbeda dari ESP32 biasa (GPIO21/22)
  Wire.begin(PIN_SDA, PIN_SCL);

  // Mulai BH1750 dalam mode continuous high-resolution (default)
  if (lightMeter.begin()) {
    Serial.println("[SENSOR] BH1750 OK");
  } else {
    Serial.println("[SENSOR] BH1750 GAGAL! Cek kabel SDA/SCL");
  }

  // Mulai DHT11
  dht.begin();
  Serial.println("[SENSOR] DHT11 OK");

  // Rain sensor: pin input digital
  // GPIO5 pada ESP32-S3 mendukung input digital
  pinMode(PIN_RAIN_DO, INPUT);
  Serial.println("[SENSOR] Rain Sensor OK");
}

SensorData sensorsRead() {
  SensorData data;
  data.sensorError = false;

  // === Baca DHT11 (suhu + kelembapan) ===
  // DHT11 butuh minimal 1 detik antar pembacaan
  // Kalau gagal, readTemperature() dan readHumidity() return NaN
  data.temperature = dht.readTemperature();  // Celsius
  data.humidity = dht.readHumidity();        // Persen

  if (isnan(data.temperature) || isnan(data.humidity)) {
    // NaN = sensor gagal baca (kabel putus, timing error, dll)
    data.temperature = -1;
    data.humidity = -1;
    data.sensorError = true;
    Serial.println("[ERROR] DHT11 gagal baca! Cek kabel DATA di GPIO4");
  }

  // === Baca BH1750 (intensitas cahaya) ===
  // Return nilai lux. Kalau gagal, return nilai negatif
  data.lux = lightMeter.readLightLevel();

  if (data.lux < 0) {
    data.lux = -1;
    data.sensorError = true;
    Serial.println("[ERROR] BH1750 gagal baca! Cek kabel SDA(GPIO8)/SCL(GPIO9)");
  }

  // === Baca Rain Sensor (digital) ===
  // digitalRead: LOW = hujan (sesuai RAIN_DETECTED di config.h)
  // PENTING: Kita HANYA pakai pin DO (digital output), BUKAN AO (analog)
  //   AO bisa output sampai 5V → merusak ADC ESP32 yang max 3.3V
  data.isRaining = (digitalRead(PIN_RAIN_DO) == RAIN_DETECTED);

  return data;
}
