#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

// Mode kipas
enum FanMode {
  FAN_AUTO,    // Otomatis: nyala saat overheat, mati kalau adem
  FAN_MANUAL   // Manual: ikutin perintah user (ON/OFF), abaikan suhu
};

// Inisialisasi pin kipas (panggil sekali di setup())
void fanInit();

// Set mode AUTO/MANUAL
void fanSetMode(FanMode mode);

// Set kipas ON/OFF (otomatis pindah ke MODE_MANUAL)
void fanSetManual(bool on);

// Update kipas berdasarkan kondisi suhu (hanya berpengaruh kalau mode AUTO)
// Panggil dari decisionProcess setiap baca sensor
void fanUpdateAuto(bool overheating);

// Status saat ini
bool fanIsOn();
FanMode fanGetMode();

#endif
