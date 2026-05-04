#include "servo_control.h"
#include "config.h"
#include <ESP32Servo.h>

// Object servo
Servo roofServo;

// State tracking
int currentAngle = SERVO_CLOSED;     // Posisi servo saat ini (mulai dari TUTUP)
int pendingAngle = SERVO_CLOSED;     // Target yang sedang menunggu hysteresis
unsigned long lastChangeRequest = 0; // Waktu terakhir target berubah
RoofState state = ROOF_CLOSED;       // Status atap saat ini

// Update LED merah sebagai indikator atap: nyala = TUTUP, mati = BUKA
static void updateRoofLed(int angle) {
  bool isClosed = (angle == SERVO_CLOSED);
  digitalWrite(PIN_LED_RED, isClosed ? HIGH : LOW);
}

void servoInit() {
  pinMode(PIN_LED_RED, OUTPUT);

  // Servo opsional — attach kalau servo dipasang, aman juga kalau tidak.
  roofServo.attach(PIN_SERVO);

  // Failsafe: posisi awal TUTUP
  roofServo.write(SERVO_CLOSED);
  currentAngle = SERVO_CLOSED;
  pendingAngle = SERVO_CLOSED;
  state = ROOF_CLOSED;
  updateRoofLed(currentAngle);

  Serial.println("[SERVO] Inisialisasi selesai, posisi awal: TUTUP (LED merah ON)");
}

void servoSetTarget(int newTarget) {
  // Clamp ke range valid (0-180)
  if (newTarget < SERVO_CLOSED) newTarget = SERVO_CLOSED;
  if (newTarget > SERVO_OPEN) newTarget = SERVO_OPEN;

  // Kalau target baru BERBEDA dari yang sedang pending → reset timer hysteresis
  // Ini yang mencegah jitter: kalau kondisi berubah-ubah terus dalam 60 detik,
  // servo tidak akan bergerak sampai kondisi STABIL selama 60 detik penuh
  if (newTarget != pendingAngle) {
    pendingAngle = newTarget;
    lastChangeRequest = millis();

    if (newTarget != currentAngle) {
      state = ROOF_TRANSITIONING;
      Serial.printf("[SERVO] Target baru: %d derajat — tunggu hysteresis %d detik\n",
        newTarget, HYSTERESIS_DELAY / 1000);
    }
  }
}

bool servoUpdate() {
  // Kalau pending == current, tidak ada yang perlu dilakukan
  if (pendingAngle == currentAngle) {
    return false;
  }

  // Cek apakah sudah lewat waktu hysteresis (60 detik)
  // Menggunakan aritmetika unsigned → otomatis handle millis() overflow (~49 hari)
  if (millis() - lastChangeRequest >= HYSTERESIS_DELAY) {
    // Hysteresis tercapai! Gerakkan servo
    currentAngle = pendingAngle;
    roofServo.write(currentAngle);
    updateRoofLed(currentAngle);

    // Update state
    if (currentAngle == SERVO_OPEN) {
      state = ROOF_OPEN;
    } else {
      state = ROOF_CLOSED;
    }

    Serial.printf("[SERVO] >>> BERGERAK ke %d derajat — status: %s\n",
      currentAngle,
      (state == ROOF_OPEN) ? "BUKA" : "TUTUP"
    );

    return true;  // Servo baru saja bergerak
  }

  return false;  // Masih menunggu hysteresis
}

int servoGetCurrentAngle() {
  return currentAngle;
}

RoofState servoGetState() {
  return state;
}
