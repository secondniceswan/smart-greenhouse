#include "fan_control.h"
#include "config.h"
#include <Arduino.h>

static bool fanOn = false;
static FanMode mode = FAN_AUTO;
static bool stateChanged = false;

static void writeFan(bool on) {
  if (on != fanOn) stateChanged = true;
  digitalWrite(PIN_FAN, on ? HIGH : LOW);
  fanOn = on;
}

void fanInit() {
  pinMode(PIN_FAN, OUTPUT);
  mode = FAN_AUTO;
  writeFan(false);
  Serial.println("[FAN] Inisialisasi selesai, kipas MATI, mode AUTO");
}

void fanSetMode(FanMode m) {
  mode = m;
  Serial.printf("[FAN] Mode: %s\n", (m == FAN_AUTO) ? "AUTO" : "MANUAL");
}

void fanSetManual(bool on) {
  mode = FAN_MANUAL;
  writeFan(on);
  Serial.printf("[FAN] Manual: kipas %s\n", on ? "NYALA" : "MATI");
}

void fanUpdateAuto(bool overheating) {
  if (mode != FAN_AUTO) return;

  bool target = overheating;
  if (target != fanOn) {
    writeFan(target);
    Serial.printf("[FAN] Auto: kipas %s (overheat=%d)\n",
      target ? "NYALA" : "MATI", overheating);
  }
}

bool fanIsOn() { return fanOn; }
FanMode fanGetMode() { return mode; }

bool fanStateChangedSinceLastPost() {
  bool c = stateChanged;
  stateChanged = false;
  return c;
}
