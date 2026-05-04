#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

// Status posisi atap greenhouse
enum RoofState {
  ROOF_OPEN,          // Atap terbuka penuh (servo di 180°)
  ROOF_CLOSED,        // Atap tertutup penuh (servo di 0°)
  ROOF_TRANSITIONING  // Sedang menunggu hysteresis (belum bergerak)
};

// Inisialisasi servo (panggil sekali di setup())
// Servo mulai di posisi TUTUP (0°) sebagai failsafe
void servoInit();

// Set target sudut servo BARU
// Servo BELUM langsung bergerak — harus menunggu hysteresis (60 detik stabil)
// Kalau target berubah sebelum 60 detik, timer di-reset
void servoSetTarget(int targetAngle);

// Panggil di setiap loop()
// Mengecek apakah hysteresis sudah tercapai, kalau iya gerakkan servo
// Return true kalau servo BARU SAJA bergerak di panggilan ini
bool servoUpdate();

// Getter: sudut servo saat ini (0-180)
int servoGetCurrentAngle();

// Getter: status atap saat ini
RoofState servoGetState();

#endif
