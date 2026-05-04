#ifndef SENSORS_H
#define SENSORS_H

#include <BH1750.h>
#include <DHT.h>

// Struct untuk menyimpan semua data sensor dalam satu paket
struct SensorData {
  float temperature;    // Suhu dalam °C (dari DHT11)
  float humidity;       // Kelembapan dalam % (dari DHT11)
  float lux;            // Intensitas cahaya dalam lux (dari BH1750)
  bool isRaining;       // true = hujan, false = tidak hujan (dari Rain Sensor DO)
  bool sensorError;     // true kalau ada sensor yang gagal baca
};

// Inisialisasi semua sensor (panggil sekali di setup())
void sensorsInit();

// Baca semua sensor sekaligus, return struct SensorData
SensorData sensorsRead();

#endif
