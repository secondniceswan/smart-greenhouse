#include "decision.h"
#include "config.h"

// State internal decision engine
OperationMode currentMode = MODE_AUTO;
bool overheating = false;
bool wifiConnected = false;
bool forecastRain = false;
int forecastRainProbability = 0;

void decisionInit() {
  currentMode = MODE_AUTO;
  overheating = false;
  wifiConnected = false;
  forecastRain = false;
  forecastRainProbability = 0;
  Serial.println("[DECISION] Engine siap, mode: AUTO");
}

void decisionSetMode(OperationMode newMode) {
  currentMode = newMode;
  Serial.printf("[DECISION] Mode diubah ke: %s\n",
    (newMode == MODE_AUTO) ? "AUTO" : "MANUAL");
}

void decisionSetForecast(bool willRain, int probability) {
  forecastRain = willRain;
  forecastRainProbability = probability;
}

void decisionSetWifiStatus(bool connected) {
  wifiConnected = connected;
}

bool decisionManualOverride(int targetAngle, SensorData currentSensors) {
  // RULE 1: LOCAL IS KING
  // Kalau hujan secara fisik, perintah manual DITOLAK
  // Tidak peduli user mau buka atap — hujan = tetap tutup
  if (currentSensors.isRaining && targetAngle == SERVO_OPEN) {
    Serial.println("[DECISION] Manual override DITOLAK — hujan terdeteksi, atap tetap TUTUP");
    return false;
  }

  // Perintah manual diterima
  currentMode = MODE_MANUAL;
  servoSetTarget(targetAngle);
  Serial.printf("[DECISION] Manual override DITERIMA — target: %d derajat\n", targetAngle);
  return true;
}

SystemStatus decisionProcess(SensorData data) {
  SystemStatus status;
  status.sensors = data;
  status.mode = currentMode;
  status.wifiConnected = wifiConnected;
  status.forecastRain = forecastRain;
  status.forecastRainProbability = forecastRainProbability;

  // ==========================================================================
  // RULE 1: LOCAL IS KING — Hujan fisik = TUTUP ATAP (tidak bisa di-override)
  // ==========================================================================
  // Rain sensor mendeteksi air secara langsung di plate sensor.
  // Ini data paling akurat yang kita punya — lebih akurat dari API cuaca,
  // lebih akurat dari prediksi, dan lebih penting dari perintah manual.
  if (data.isRaining) {
    servoSetTarget(SERVO_CLOSED);

    // RULE 4: ANTI-OVEN EFFECT
    // Kalau atap tertutup (karena hujan) TAPI suhu di dalam greenhouse > 33°C,
    // tanaman bisa "matang" seperti di dalam oven.
    // Kita TIDAK boleh buka atap (masih hujan!), tapi kita BISA kasih warning.
    if (!data.sensorError && data.temperature > TEMP_OVERHEAT) {
      overheating = true;
      Serial.printf("[WARNING] ANTI-OVEN! Hujan tapi suhu %.1f C > %.1f C!\n",
        data.temperature, TEMP_OVERHEAT);
    } else {
      overheating = false;
    }
  }
  // ==========================================================================
  // Tidak hujan secara fisik — keputusan berdasarkan prioritas
  // ==========================================================================
  else {
    overheating = false;

    // Kalau mode MANUAL, keputusan sudah di-handle oleh decisionManualOverride()
    // Jadi di sini kita hanya proses kalau mode AUTO
    if (currentMode == MODE_AUTO) {

      // PRIORITAS 2: Prediksi cuaca dari Open-Meteo
      // Kalau probabilitas hujan >= 60%, tutup atap sebagai antisipasi
      // meskipun sensor hujan belum mendeteksi air
      if (forecastRain) {
        servoSetTarget(SERVO_CLOSED);
        Serial.printf("[DECISION] Tutup atap — prediksi hujan %d%%\n",
          forecastRainProbability);
      }
      // PRIORITAS 3: Keputusan berdasarkan sensor lokal
      else {
        // Cahaya terik + suhu aman → BUKA ATAP (kondisi ideal untuk tanaman)
        if (data.lux > LUX_HIGH && data.temperature < TEMP_OVERHEAT) {
          servoSetTarget(SERVO_OPEN);
        }
        // Cahaya sedang (500-10000 lux) → BUKA ATAP (masih cukup cahaya)
        else if (data.lux >= LUX_LOW) {
          servoSetTarget(SERVO_OPEN);
        }
        // Cahaya rendah (< 500 lux) → TUTUP ATAP (gelap/malam, tidak ada manfaat buka)
        else if (data.lux >= 0 && data.lux < LUX_LOW) {
          servoSetTarget(SERVO_CLOSED);
        }
        // Kalau lux = -1 (sensor error), jangan ambil keputusan
        // Biarkan servo di posisi terakhir — lebih aman daripada gerak berdasarkan data salah
      }
    }
  }

  // Update servo (hysteresis timer dicek di dalam servoUpdate)
  servoUpdate();

  // Kipas otomatis: nyala saat overheating (kalau mode AUTO)
  fanUpdateAuto(overheating);

  // Isi status output
  status.roofState = servoGetState();
  status.roofAngle = servoGetCurrentAngle();
  status.overheating = overheating;
  status.fanOn = fanIsOn();
  status.fanMode = fanGetMode();

  return status;
}
