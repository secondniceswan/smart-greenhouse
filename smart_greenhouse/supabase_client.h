#ifndef SUPABASE_CLIENT_H
#define SUPABASE_CLIENT_H

#include "decision.h"

// Kirim data sensor terbaru ke tabel "sensor_logs" di Supabase
// Return true kalau berhasil (HTTP 201)
bool supabasePostSensorData(SystemStatus status);

// Konstanta hasil supabaseCheckCommand
#define CMD_NONE          -1   // Tidak ada perintah / error
#define CMD_AUTO          -2   // Atap kembali ke mode auto
#define CMD_FAN_ON        -3   // Kipas nyala (manual)
#define CMD_FAN_OFF       -4   // Kipas mati (manual)
#define CMD_FAN_AUTO      -5   // Kipas kembali ke mode auto
// Selain itu: SERVO_OPEN (180) atau SERVO_CLOSED (0) untuk atap

// Cek perintah manual terbaru dari tabel "commands" di Supabase
int supabaseCheckCommand();

// Tandai perintah sudah dieksekusi di Supabase (set executed = true)
// Supaya perintah yang sama tidak dieksekusi ulang
bool supabaseMarkCommandDone(int commandId);

#endif
