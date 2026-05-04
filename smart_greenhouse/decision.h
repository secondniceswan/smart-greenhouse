#ifndef DECISION_H
#define DECISION_H

#include "sensors.h"
#include "servo_control.h"

// Mode operasi sistem
enum OperationMode {
  MODE_AUTO,    // Otomatis berdasarkan sensor + prediksi cuaca
  MODE_MANUAL   // Manual dari perintah user via Supabase
};

// Status lengkap sistem pada satu titik waktu
struct SystemStatus {
  SensorData sensors;           // Data dari semua sensor
  RoofState roofState;          // Status atap (OPEN/CLOSED/TRANSITIONING)
  int roofAngle;                // Sudut servo saat ini (0-180)
  OperationMode mode;           // Mode operasi (AUTO/MANUAL)
  bool overheating;             // true = anti-oven warning aktif
  bool wifiConnected;           // true = WiFi terhubung
  bool forecastRain;            // true = prediksi cuaca akan hujan
  int forecastRainProbability;  // Probabilitas hujan dari Open-Meteo (%)
};

// Inisialisasi decision engine (panggil sekali di setup())
void decisionInit();

// Proses keputusan berdasarkan data sensor
// Ini adalah "otak" sistem — menentukan buka/tutup atap
// Return: SystemStatus lengkap
SystemStatus decisionProcess(SensorData sensorData);

// Set mode operasi (dipanggil saat ada perintah manual dari Supabase)
void decisionSetMode(OperationMode newMode);

// Set data forecast (dipanggil setelah berhasil fetch Open-Meteo)
void decisionSetForecast(bool willRain, int probability);

// Set status WiFi (dipanggil dari wifi_manager)
void decisionSetWifiStatus(bool connected);

// Override manual: set target servo langsung (untuk perintah dari Supabase)
// Return false kalau di-block oleh Rule 1 (hujan = tetap tutup)
bool decisionManualOverride(int targetAngle, SensorData currentSensors);

#endif
