#ifndef SUPABASE_CLIENT_H
#define SUPABASE_CLIENT_H

#include "decision.h"

// Kirim data sensor terbaru ke tabel "sensor_logs" di Supabase
// Return true kalau berhasil (HTTP 201)
bool supabasePostSensorData(SystemStatus status);

// Cek perintah manual terbaru dari tabel "commands" di Supabase
// Return:
//   SERVO_OPEN (180)  = perintah buka atap
//   SERVO_CLOSED (0)  = perintah tutup atap
//   -2                = perintah kembali ke mode auto
//   -1                = tidak ada perintah baru / error
int supabaseCheckCommand();

// Tandai perintah sudah dieksekusi di Supabase (set executed = true)
// Supaya perintah yang sama tidak dieksekusi ulang
bool supabaseMarkCommandDone(int commandId);

#endif
