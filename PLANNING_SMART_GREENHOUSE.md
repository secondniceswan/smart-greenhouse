# SMART GREENHOUSE IoT — PANDUAN LENGKAP PEMBUATAN

> Dokumen ini adalah panduan **end-to-end** untuk membuat Smart Greenhouse berbasis ESP32-WROVER-E.
> Dari beli komponen, rakit hardware, coding firmware, setup database, sampai deploy web dashboard.
> Cukup ikuti dokumen ini dari atas ke bawah.

---

## DAFTAR ISI

1. [Gambaran Umum Sistem](#1-gambaran-umum-sistem)
2. [Arsitektur & Alur Data](#2-arsitektur--alur-data)
3. [Aturan Logika Kritis](#3-aturan-logika-kritis)
4. [Daftar Komponen (BOM)](#4-daftar-komponen-bom)
5. [Wiring / Rangkaian Elektrikal](#5-wiring--rangkaian-elektrikal)
6. [Perakitan Fisik Step-by-Step](#6-perakitan-fisik-step-by-step)
7. [Setup Development Environment](#7-setup-development-environment)
8. [Phase 1 — Firmware Lokal (Sensor + Servo)](#8-phase-1--firmware-lokal-sensor--servo)
9. [Phase 2 — WiFi + Supabase Integration](#9-phase-2--wifi--supabase-integration)
10. [Phase 3 — Web Dashboard (Vercel)](#10-phase-3--web-dashboard-vercel)
11. [Phase 4 — Open-Meteo API Integration](#11-phase-4--open-meteo-api-integration)
12. [Database Schema (Supabase)](#12-database-schema-supabase)
13. [API Endpoint Reference](#13-api-endpoint-reference)
14. [State Machine & Decision Tree](#14-state-machine--decision-tree)
15. [Penanganan Error & Edge Cases](#15-penanganan-error--edge-cases)
16. [Testing & Validasi](#16-testing--validasi)
17. [Estimasi Kelistrikan & Power Budget](#17-estimasi-kelistrikan--power-budget)
18. [Timeline Pengerjaan](#18-timeline-pengerjaan)
19. [Troubleshooting](#19-troubleshooting)
20. [Referensi Datasheet](#20-referensi-datasheet)

---

## 1. GAMBARAN UMUM SISTEM

### Apa ini?
Greenhouse mini otomatis yang bisa buka/tutup atap louver berdasarkan:
- Kondisi cuaca lokal (sensor hujan, cahaya, suhu, kelembapan)
- Prediksi cuaca dari internet (Open-Meteo API)
- Perintah manual dari user via web dashboard

### Tujuan
- Atap **terbuka** saat cuaca cerah → tanaman dapat sinar matahari
- Atap **tertutup** saat hujan → tanaman terlindungi
- User bisa **override manual** kapan saja via web
- Sistem tetap **jalan offline** kalau WiFi mati

### Komponen Utama
```
[SENSOR] → [ESP32-WROVER-E] → [SERVO MG996R]
                ↕ WiFi
          [SUPABASE] ← [OPEN-METEO API]
                ↕
          [WEB DASHBOARD / VERCEL]
```

---

## 2. ARSITEKTUR & ALUR DATA

### Diagram Alur

```
┌─────────────────────────────────────────────────────────────────┐
│                        CLOUD LAYER                              │
│                                                                 │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐      │
│  │  OPEN-METEO  │    │   SUPABASE   │    │   VERCEL     │      │
│  │  Weather API │    │  PostgreSQL  │    │  Next.js App │      │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘      │
│         │ GET               │ REST API          │ READ         │
│         │ forecast          │ POST/GET          │ only from    │
│         │                   │                   │ Supabase     │
└─────────┼───────────────────┼───────────────────┼──────────────┘
          │                   │                   │
          ▼                   ▼                   │
┌─────────────────────────────────────────────────┘              │
│                    ESP32-WROVER-E                                │
│                                                                 │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌──────────────┐      │
│  │ BH1750  │  │  DHT11  │  │  RAIN   │  │   MG996R     │      │
│  │ Cahaya  │  │Suhu/Hum │  │ Sensor  │  │   SERVO      │      │
│  │ (I2C)   │  │(Digital)│  │(Digital)│  │  (PWM)       │      │
│  └────┬────┘  └────┬────┘  └────┬────┘  └──────┬───────┘      │
│       │            │            │               │              │
│       ▼            ▼            ▼               ▲              │
│  ┌──────────────────────────────────────────────┐              │
│  │           DECISION ENGINE (firmware)          │              │
│  │                                               │              │
│  │  1. Baca semua sensor                        │              │
│  │  2. Cek override manual dari Supabase        │              │
│  │  3. Cek forecast dari Open-Meteo             │              │
│  │  4. Terapkan aturan prioritas                │              │
│  │  5. Gerakkan servo kalau perlu               │              │
│  │  6. Kirim data ke Supabase                   │              │
│  └──────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────────┘
```

### Alur Data Detail

```
SETIAP 5 DETIK:
  ├── Baca BH1750 → lux (0-65535)
  ├── Baca DHT11 → suhu (°C), kelembapan (%)
  └── Baca Rain Sensor DO → hujan (true/false)

SETIAP 10 DETIK:
  └── GET Supabase → cek ada perintah manual baru?
        ├── Ada → eksekusi perintah (buka/tutup/auto)
        └── Tidak → lanjut mode otomatis

SETIAP 60 DETIK:
  └── POST ke Supabase → kirim data sensor terbaru
        {suhu, kelembapan, lux, hujan, posisi_atap, mode, timestamp}

SETIAP 30 MENIT:
  └── GET Open-Meteo API → prediksi cuaca 6 jam ke depan
        └── Simpan: akan_hujan (bool), suhu_prediksi (°C)

KAPAN SAJA (event-driven):
  └── Decision Engine → hitung posisi atap → gerakkan servo
```

---

## 3. ATURAN LOGIKA KRITIS

### Rule 1: LOCAL IS KING (Prioritas Tertinggi)
```
JIKA rain_sensor == HUJAN:
    → TUTUP ATAP (servo 0°) — TIDAK BISA DI-OVERRIDE
    → Alasan: Data fisik real-time lebih akurat dari apapun
```
Sensor hujan mendeteksi air secara fisik. Tidak ada API, prediksi, atau perintah
manual yang boleh membatalkan ini. Ini safety rule nomor 1.

### Rule 2: HYSTERESIS / ANTI-JITTER
```
JIKA kondisi berubah (misal: hujan → cerah):
    → Tunggu 60 detik kondisi STABIL
    → BARU gerakkan servo
    → Gunakan millis(), JANGAN delay()
```
Tanpa hysteresis, hujan rintik-rintik bikin servo buka-tutup-buka-tutup terus
(jittery). Ini bisa rusak servo dan mekanik atap.

**Implementasi:**
```
variabel: last_state_change_time
variabel: pending_state
variabel: current_state

JIKA sensor_state != current_state:
    JIKA sensor_state != pending_state:
        pending_state = sensor_state
        last_state_change_time = millis()
    JIKA millis() - last_state_change_time >= 60000:
        current_state = pending_state
        gerakkan_servo(current_state)
```

### Rule 3: FAILSAFE OFFLINE
```
JIKA WiFi putus:
    → Jangan crash, jangan restart
    → Lanjut operasi dengan data sensor lokal saja
    → Skip semua HTTP request
    → Coba reconnect setiap 30 detik (non-blocking)
    → Kalau reconnect berhasil, resume normal
```

**Implementasi:**
```
variabel: wifi_connected (bool)
variabel: last_reconnect_attempt (unsigned long)

DI SETIAP LOOP:
    JIKA WiFi.status() != WL_CONNECTED:
        wifi_connected = false
        JIKA millis() - last_reconnect_attempt >= 30000:
            WiFi.reconnect()  // non-blocking
            last_reconnect_attempt = millis()
    ELSE:
        wifi_connected = true

    // Sensor + servo logic SELALU jalan
    baca_sensor()
    decision_engine()

    // HTTP request HANYA kalau online
    JIKA wifi_connected:
        kirim_data_supabase()
        cek_perintah_manual()
        ambil_forecast()
```

### Rule 4: ANTI-OVEN EFFECT
```
JIKA atap TERTUTUP (karena hujan) DAN suhu > 33°C:
    → JANGAN buka atap (masih hujan!)
    → Nyalakan WARNING (LED merah berkedip)
    → Kirim flag "overheating: true" ke Supabase
    → Dashboard tampilkan peringatan
```
Greenhouse tertutup + matahari terik = efek oven. Tanaman bisa mati kepanasan.
Tapi kalau hujan, kita tetap tidak boleh buka atap. Solusinya: warning system
supaya user tahu dan bisa pasang ventilasi/kipas manual.

---

## 4. DAFTAR KOMPONEN (BOM)

### 4.1 Komponen Utama

| No | Komponen | Model/Seri | Spesifikasi | Qty | Keyword Shopee | Est. Harga |
|----|----------|-----------|-------------|-----|---------------|------------|
| 1 | Microcontroller | **ESP32-WROVER-E DevKit** | Dual-core 240MHz, 520KB SRAM + 4MB PSRAM, 4MB Flash, WiFi+BT | 1 | `ESP32-WROVER-E development board` | 75-110rb |
| 2 | Sensor Cahaya | **GY-302 (BH1750FVI)** | I2C, 1-65535 lux, 3.3-5V | 1 | `BH1750 GY-302` | 8-15rb |
| 3 | Sensor Suhu/Kelembapan | **DHT11 Module (3-pin)** | Digital, 0-50°C, 20-90% RH, 3.3-5V | 1 | `DHT11 module 3 pin` | 8-15rb |
| 4 | Sensor Hujan | **MH-RD + LM393 Module** | Digital + Analog output, 3.3-5V | 1 | `raindrop sensor module` | 10-18rb |
| 5 | Servo Motor | **MG996R** | Metal gear, 10kg·cm torsi, 4.8-7.2V, ~2.5A stall | 1 | `MG996R servo motor` | 35-55rb |

### 4.2 Power Supply

| No | Komponen | Spesifikasi | Qty | Keyword Shopee | Est. Harga |
|----|----------|-------------|-----|---------------|------------|
| 6 | Adaptor DC | **5V 3A**, Jack DC 5.5x2.1mm, input 220V AC | 1 | `adaptor 5V 3A DC` | 20-35rb |
| 7 | Jack DC Female | 5.5x2.1mm female → screw terminal | 1 | `jack DC female screw terminal` | 3-5rb |

### 4.3 Kabel & Koneksi

| No | Komponen | Spesifikasi | Qty | Keyword Shopee | Est. Harga |
|----|----------|-------------|-----|---------------|------------|
| 8 | Jumper Male-Male | 20cm, 40pcs/set | 1 set | `kabel jumper male male 20cm` | 8-12rb |
| 9 | Jumper Male-Female | 20cm, 40pcs/set | 1 set | `kabel jumper male female 20cm` | 8-12rb |
| 10 | Kabel Micro USB | Micro USB to USB-A, **data + charge** | 1 | `kabel micro USB data` | 8-15rb |
| 11 | Breadboard | Full-size 830 tie points | 1 | `breadboard 830` | 12-20rb |

### 4.4 Komponen Pendukung

| No | Komponen | Spesifikasi | Qty | Keyword Shopee | Est. Harga |
|----|----------|-------------|-----|---------------|------------|
| 12 | LED Merah 5mm | Indikator warning anti-oven | 2 | `LED 5mm merah` | 1-2rb |
| 13 | Resistor 220 ohm | Current limiter untuk LED | 2 | `resistor 220 ohm` | 1-2rb |
| 14 | Capacitor Elektrolit | **470µF 16V** — stabilkan power servo | 1 | `capacitor 470uF 16V` | 2-5rb |
| 15 | Terminal Block | 2-pin PCB mount | 2 | `terminal block 2 pin PCB` | 3-5rb |

### 4.5 Total Estimasi Budget
```
Komponen utama   : Rp 136.000 - 213.000
Power supply     : Rp  23.000 -  40.000
Kabel & koneksi  : Rp  36.000 -  59.000
Pendukung        : Rp   7.000 -  14.000
─────────────────────────────────────────
TOTAL            : Rp 202.000 - 326.000
```

---

## 5. WIRING / RANGKAIAN ELEKTRIKAL

### 5.1 Skema Pin Assignment

```
ESP32-WROVER-E DevKit
┌─────────────────────────────────────────────────┐
│                                                 │
│  VIN  ←── Adaptor +5V (via jack DC)             │
│  GND  ←── Adaptor GND (COMMON GROUND)          │
│  3.3V ──→ VCC BH1750, VCC DHT11, VCC Rain      │
│                                                 │
│  GPIO21 (SDA) ──→ BH1750 SDA                   │
│  GPIO22 (SCL) ──→ BH1750 SCL                   │
│  GPIO4        ──→ DHT11 DATA                    │
│  GPIO34       ──→ Rain Sensor DO (digital only) │
│  GPIO13       ──→ Servo MG996R Signal (oranye)  │
│  GPIO2        ──→ LED Warning (via R 220 ohm)   │
│                                                 │
└─────────────────────────────────────────────────┘
```

### 5.2 Diagram Wiring Lengkap

```
                    ADAPTOR DC 5V 3A
                    ┌─────────────┐
                    │  +5V   GND  │
                    └──┬──────┬───┘
                       │      │
          ┌────────────┤      ├──────────────────────────────┐
          │            │      │                              │
          │     ┌──────┘      └────────┐                     │
          │     │                      │                     │
          │     │  ESP32-WROVER-E      │                     │
          │     │  ┌──────────────┐    │                     │
          │     └──► VIN          │    │                     │
          │        │              │    │                     │
          │  ┌─────┤ 3.3V        GND ◄┘                     │
          │  │     │              │                           │
          │  │     │ GPIO21(SDA)──┼──────────► BH1750 SDA    │
          │  │     │ GPIO22(SCL)──┼──────────► BH1750 SCL    │
          │  │     │ GPIO4  ──────┼──────────► DHT11 DATA    │
          │  │     │ GPIO34 ──────┼──────────► Rain DO       │
          │  │     │ GPIO13 ──────┼──────────► Servo SIG     │
          │  │     │ GPIO2  ──────┼──► R220Ω ──► LED ──► GND │
          │  │     └──────────────┘                           │
          │  │                                               │
          │  │  ┌─── 3.3V BUS ─────────────────────┐        │
          │  │  │                                   │        │
          │  └──┼──► BH1750 VCC                     │        │
          │     ├──► DHT11 VCC                      │        │
          │     └──► Rain Module VCC                │        │
          │                                         │        │
          │  ┌─── GND BUS ─────────────────────┐    │        │
          │  │                                  │    │        │
          │  ├──► BH1750 GND                    │    │        │
          │  ├──► DHT11 GND                     │    │        │
          │  ├──► Rain Module GND               │    │        │
          │  ├──► Servo GND (coklat)            │    │        │
          │  └──► LED GND                       │    │        │
          │                                          │        │
          │         ┌──────┐                         │        │
          └────┬────┤470µF ├────┬────► Servo VCC (merah)     │
               │    │ 16V  │    │                             │
               │    └──────┘    │                             │
               │  (+)      (-) │                             │
               │    capacitor   │                             │
               │                └─────────────────────────────┘
               │                        (common GND)
               └── dari +5V adaptor
```

### 5.3 Aturan Wiring KRITIS

| Aturan | Penjelasan |
|--------|-----------|
| **Semua sensor pakai 3.3V** | Dari pin 3.3V ESP32. JANGAN dari 5V. |
| **Servo power dari adaptor** | Langsung dari +5V adaptor, BUKAN dari pin ESP32. Pin 5V ESP32 max ~500mA, servo butuh 2.5A. |
| **Rain sensor: DO saja** | Pakai pin DO (digital output). JANGAN sambung AO (analog) — bisa output 5V, rusak ADC ESP32 yang max 3.3V. |
| **Common ground WAJIB** | GND adaptor, GND ESP32, GND semua sensor, GND servo — HARUS terhubung semua ke satu titik GND. Tanpa common ground, sinyal kacau. |
| **Capacitor di servo** | Pasang 470µF (kaki + ke +5V, kaki - ke GND) sedekat mungkin dengan servo. Ini buffer lonjakan arus saat servo mulai bergerak. |
| **Kabel USB = data** | Kabel micro USB HARUS yang support data, bukan charge-only. Test: colok ke PC, cek Device Manager muncul COM port. |

---

## 6. PERAKITAN FISIK STEP-BY-STEP

### Step 1: Siapkan Breadboard
```
Tancapkan ESP32-WROVER-E DevKit di tengah breadboard.
Pastikan kedua sisi pin masih bisa diakses.
```

### Step 2: Power Distribution
```
1. Sambung jack DC female ke adaptor 5V 3A
2. Kabel + dari jack → breadboard power rail (+)
3. Kabel - dari jack → breadboard power rail (-)
4. Jumper dari power rail (+) → ESP32 VIN
5. Jumper dari power rail (-) → ESP32 GND
6. Jumper dari ESP32 3.3V → breadboard rail baris lain (ini jadi 3.3V bus)
7. Jumper dari ESP32 GND → breadboard rail baris lain (ini jadi GND bus)
```

### Step 3: Pasang BH1750
```
1. VCC → 3.3V bus
2. GND → GND bus
3. SDA → GPIO21
4. SCL → GPIO22
5. ADDR → biarkan floating (default address 0x23)
```

### Step 4: Pasang DHT11
```
1. VCC → 3.3V bus
2. GND → GND bus
3. DATA → GPIO4
(Module sudah ada pull-up resistor internal)
```

### Step 5: Pasang Rain Sensor
```
1. Sambung kabel dari rain detection plate ke modul LM393
2. VCC modul → 3.3V bus
3. GND modul → GND bus
4. DO modul → GPIO34
5. AO modul → TIDAK DISAMBUNG (biarkan kosong)
6. Putar potensiometer di modul untuk atur sensitivity
```

### Step 6: Pasang Servo MG996R
```
1. Kabel MERAH (VCC) → power rail +5V (LANGSUNG dari adaptor)
2. Kabel COKLAT (GND) → GND bus
3. Kabel ORANYE (Signal) → GPIO13
4. Pasang capacitor 470µF:
   - Kaki panjang (+) → power rail +5V
   - Kaki pendek (-) → GND bus
   - Posisi: sedekat mungkin dengan sambungan servo
```

### Step 7: Pasang LED Warning
```
1. GPIO2 → Resistor 220Ω → Kaki panjang LED (+/anoda)
2. Kaki pendek LED (-/katoda) → GND bus
```

### Step 8: Verifikasi Sebelum Power On
```
CHECKLIST:
[ ] Semua GND terhubung ke satu bus
[ ] Tidak ada kabel 5V yang masuk ke pin sensor
[ ] Rain sensor AO tidak tersambung ke manapun
[ ] Servo power dari adaptor, bukan dari ESP32
[ ] Capacitor polaritas benar (+ ke 5V, - ke GND)
[ ] ESP32 belum di-power (belum colok adaptor/USB)
```

### Step 9: First Power On
```
1. Colok adaptor 5V 3A ke listrik
2. Cek: LED power ESP32 menyala
3. Cek: tidak ada komponen yang panas (sentuh hati-hati setelah 10 detik)
4. Kalau ada yang panas → CABUT SEGERA → cek wiring
```

---

## 7. SETUP DEVELOPMENT ENVIRONMENT

### 7.1 Software yang Dibutuhkan

| Software | Fungsi | Download |
|----------|--------|----------|
| Arduino IDE 2.x | Compile & upload firmware ke ESP32 | arduino.cc |
| Node.js 18+ | Untuk frontend Next.js | nodejs.org |
| Git | Version control | git-scm.com |
| Browser modern | Akses Supabase dashboard & web app | - |

### 7.2 Setup Arduino IDE

**Install ESP32 Board:**
```
1. File → Preferences → Additional Board Manager URLs:
   https://espressif.github.io/arduino-esp32/package_esp32_index.json

2. Tools → Board → Board Manager → cari "esp32" → Install "esp32 by Espressif"

3. Tools → Board → ESP32 Arduino → pilih "ESP32 Wrover Module"

4. Tools → PSRAM → "Enabled"

5. Tools → Upload Speed → 921600

6. Tools → Flash Size → "4MB (32Mb)"

7. Tools → Port → pilih COM port yang muncul saat ESP32 dicolok USB
```

**Install Library:**
```
Tools → Manage Libraries → Install:
  - "BH1750" by Christopher Laws (sensor cahaya)
  - "DHT sensor library" by Adafruit (sensor suhu)
  - "Adafruit Unified Sensor" by Adafruit (dependency DHT)
  - "ESP32Servo" by Kevin Harrington (servo control)
  - "ArduinoJson" by Benoit Blanchon (parsing JSON)
  - "WiFiClientSecure" (sudah built-in ESP32)
  - "HTTPClient" (sudah built-in ESP32)
```

### 7.3 Setup Supabase

```
1. Buka supabase.com → Sign up / Login
2. "New Project" → nama: smart-greenhouse
3. Pilih region: Southeast Asia (Singapore)
4. Set database password → SIMPAN BAIK-BAIK
5. Catat:
   - Project URL: https://xxxxx.supabase.co
   - Anon Key: eyJhbGciOiJ... (Settings → API → anon public)
   - Service Role Key: eyJhbGciOiJ... (untuk backend saja, JANGAN taruh di frontend)
```

### 7.4 Setup Vercel + Next.js

```bash
# Buat project baru
npx create-next-app@latest greenhouse-dashboard
# Pilih: TypeScript=Yes, Tailwind=Yes, App Router=Yes

cd greenhouse-dashboard
npm install @supabase/supabase-js

# Buat .env.local
echo "NEXT_PUBLIC_SUPABASE_URL=https://xxxxx.supabase.co" >> .env.local
echo "NEXT_PUBLIC_SUPABASE_ANON_KEY=eyJhbGciOiJ..." >> .env.local
```

---

## 8. PHASE 1 — FIRMWARE LOKAL (SENSOR + SERVO)

> Tujuan: ESP32 baca semua sensor, gerakkan servo berdasarkan logika lokal.
> Tidak ada WiFi. Tidak ada internet. Murni offline.

### 8.1 Struktur File

```
smart_greenhouse/
├── smart_greenhouse.ino      // File utama, setup() dan loop()
├── config.h                  // Semua konstanta dan pin definition
├── sensors.h / sensors.cpp   // Fungsi baca sensor
├── servo_control.h / .cpp    // Fungsi kontrol servo + hysteresis
└── decision.h / .cpp         // Decision engine / logika keputusan
```

### 8.2 config.h — Konstanta & Pin

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// ==================== PIN DEFINITION ====================
#define PIN_DHT11         4      // GPIO4  → DHT11 DATA
#define PIN_RAIN_DO       34     // GPIO34 → Rain Sensor Digital Output
#define PIN_SERVO         13     // GPIO13 → Servo MG996R Signal
#define PIN_LED_WARNING   2      // GPIO2  → LED Warning Anti-Oven
// BH1750 pakai I2C default: SDA=GPIO21, SCL=GPIO22

// ==================== SERVO POSITIONS ====================
#define SERVO_OPEN        180    // Atap terbuka penuh (derajat)
#define SERVO_CLOSED      0      // Atap tertutup penuh (derajat)

// ==================== THRESHOLDS ====================
#define TEMP_OVERHEAT     33.0   // °C — trigger anti-oven warning
#define LUX_LOW           500    // Lux — dibawah ini = mendung/gelap
#define LUX_HIGH          10000  // Lux — diatas ini = terik
#define HUMIDITY_HIGH     85.0   // % — kelembapan tinggi

// ==================== TIMING (millis) ====================
#define SENSOR_READ_INTERVAL    5000     // Baca sensor tiap 5 detik
#define HYSTERESIS_DELAY        60000    // Tunggu 60 detik sebelum gerak servo
#define LED_BLINK_INTERVAL      500      // LED warning kedip tiap 500ms
#define WIFI_RECONNECT_INTERVAL 30000    // Coba reconnect tiap 30 detik

// Phase 2 timing (akan dipakai nanti)
#define SUPABASE_POST_INTERVAL   60000   // Kirim data tiap 60 detik
#define SUPABASE_CHECK_INTERVAL  10000   // Cek perintah tiap 10 detik
#define WEATHER_FETCH_INTERVAL   1800000 // Ambil forecast tiap 30 menit

// ==================== SENSOR STATE ====================
// Rain sensor: LOW = hujan (ada air di plate), HIGH = tidak hujan
// Ini tergantung modul. Cek dengan multimeter atau Serial Monitor dulu.
#define RAIN_DETECTED     LOW

#endif
```

### 8.3 sensors.h / sensors.cpp — Baca Sensor

```cpp
// sensors.h
#ifndef SENSORS_H
#define SENSORS_H

#include <BH1750.h>
#include <DHT.h>

struct SensorData {
  float temperature;    // °C dari DHT11
  float humidity;       // % dari DHT11
  float lux;            // lux dari BH1750
  bool isRaining;       // true/false dari rain sensor DO
  bool sensorError;     // true kalau ada sensor gagal baca
};

void sensorsInit();
SensorData sensorsRead();

#endif
```

```cpp
// sensors.cpp
#include "sensors.h"
#include "config.h"
#include <Wire.h>

BH1750 lightMeter;
DHT dht(PIN_DHT11, DHT11);

void sensorsInit() {
  // Inisialisasi I2C untuk BH1750
  Wire.begin();
  lightMeter.begin();

  // Inisialisasi DHT11
  dht.begin();

  // Rain sensor pin (input only, GPIO34 tidak punya pull-up internal)
  pinMode(PIN_RAIN_DO, INPUT);
}

SensorData sensorsRead() {
  SensorData data;
  data.sensorError = false;

  // Baca DHT11
  data.temperature = dht.readTemperature();
  data.humidity = dht.readHumidity();

  // Cek apakah DHT11 error (return NaN kalau gagal)
  if (isnan(data.temperature) || isnan(data.humidity)) {
    data.temperature = -1;
    data.humidity = -1;
    data.sensorError = true;
    Serial.println("[ERROR] DHT11 gagal baca!");
  }

  // Baca BH1750
  data.lux = lightMeter.readLightLevel();
  if (data.lux < 0) {
    data.lux = -1;
    data.sensorError = true;
    Serial.println("[ERROR] BH1750 gagal baca!");
  }

  // Baca Rain Sensor (digital)
  data.isRaining = (digitalRead(PIN_RAIN_DO) == RAIN_DETECTED);

  return data;
}
```

### 8.4 servo_control.h / .cpp — Kontrol Servo + Hysteresis

```cpp
// servo_control.h
#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

enum RoofState {
  ROOF_OPEN,
  ROOF_CLOSED,
  ROOF_TRANSITIONING  // Sedang menunggu hysteresis
};

void servoInit();
void servoSetTarget(int targetAngle);     // Set target, tapi servo belum gerak
bool servoUpdate();                        // Dipanggil di loop(), return true kalau servo baru saja bergerak
int servoGetCurrentAngle();
RoofState servoGetState();

#endif
```

```cpp
// servo_control.cpp
#include "servo_control.h"
#include "config.h"
#include <ESP32Servo.h>

Servo roofServo;

int currentAngle = SERVO_CLOSED;   // Mulai dari tutup (aman)
int targetAngle = SERVO_CLOSED;
int pendingAngle = SERVO_CLOSED;
unsigned long lastChangeRequest = 0;
bool targetChanged = false;
RoofState state = ROOF_CLOSED;

void servoInit() {
  roofServo.attach(PIN_SERVO);
  roofServo.write(SERVO_CLOSED);  // Posisi awal: tutup
  currentAngle = SERVO_CLOSED;
  Serial.println("[SERVO] Inisialisasi selesai, posisi: TUTUP");
}

void servoSetTarget(int newTarget) {
  // Clamp ke range valid
  newTarget = constrain(newTarget, SERVO_CLOSED, SERVO_OPEN);

  // Kalau target beda dari yang sedang pending, reset timer hysteresis
  if (newTarget != pendingAngle) {
    pendingAngle = newTarget;
    lastChangeRequest = millis();
    state = ROOF_TRANSITIONING;
    Serial.printf("[SERVO] Target baru: %d° — tunggu hysteresis 60 detik\n", newTarget);
  }
}

bool servoUpdate() {
  // Tidak ada perubahan pending
  if (pendingAngle == currentAngle) {
    return false;
  }

  // Cek apakah sudah lewat waktu hysteresis
  if (millis() - lastChangeRequest >= HYSTERESIS_DELAY) {
    currentAngle = pendingAngle;
    roofServo.write(currentAngle);
    state = (currentAngle == SERVO_OPEN) ? ROOF_OPEN : ROOF_CLOSED;
    Serial.printf("[SERVO] Bergerak ke %d° — status: %s\n",
      currentAngle,
      (state == ROOF_OPEN) ? "BUKA" : "TUTUP"
    );
    return true;
  }

  return false;
}

int servoGetCurrentAngle() {
  return currentAngle;
}

RoofState servoGetState() {
  return state;
}
```

### 8.5 decision.h / .cpp — Decision Engine

```cpp
// decision.h
#ifndef DECISION_H
#define DECISION_H

#include "sensors.h"
#include "servo_control.h"

enum OperationMode {
  MODE_AUTO,      // Otomatis berdasarkan sensor
  MODE_MANUAL     // Manual dari user (Phase 2)
};

struct SystemStatus {
  SensorData sensors;
  RoofState roofState;
  int roofAngle;
  OperationMode mode;
  bool overheating;         // Anti-oven warning aktif
  bool wifiConnected;       // Phase 2
  bool manualOverrideActive; // Phase 2
};

void decisionInit();
SystemStatus decisionProcess(SensorData sensorData);

#endif
```

```cpp
// decision.cpp
#include "decision.h"
#include "config.h"

OperationMode currentMode = MODE_AUTO;
bool overheating = false;

void decisionInit() {
  currentMode = MODE_AUTO;
  overheating = false;
}

SystemStatus decisionProcess(SensorData data) {
  SystemStatus status;
  status.sensors = data;
  status.mode = currentMode;
  status.wifiConnected = false;        // Phase 2
  status.manualOverrideActive = false;  // Phase 2

  // ====================================
  // RULE 1: LOCAL IS KING — Hujan = tutup
  // ====================================
  if (data.isRaining) {
    servoSetTarget(SERVO_CLOSED);

    // RULE 4: ANTI-OVEN — Cek suhu saat atap tertutup
    if (data.temperature > TEMP_OVERHEAT && !data.sensorError) {
      overheating = true;
      Serial.printf("[WARNING] ANTI-OVEN! Hujan tapi suhu %.1f°C > %.1f°C!\n",
        data.temperature, TEMP_OVERHEAT);
    } else {
      overheating = false;
    }
  }
  // Tidak hujan → keputusan berdasarkan cahaya dan suhu
  else {
    overheating = false;

    // Terik dan tidak terlalu panas → buka atap
    if (data.lux > LUX_HIGH && data.temperature < TEMP_OVERHEAT) {
      servoSetTarget(SERVO_OPEN);
    }
    // Mendung/gelap → tutup atap (tidak banyak manfaat buka)
    else if (data.lux < LUX_LOW) {
      servoSetTarget(SERVO_CLOSED);
    }
    // Cahaya sedang → buka atap (kondisi ideal)
    else {
      servoSetTarget(SERVO_OPEN);
    }
  }

  // Update servo (hysteresis dihandle di dalam)
  servoUpdate();

  // LED warning anti-oven
  if (overheating) {
    // Blink LED (non-blocking)
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    if (millis() - lastBlink >= LED_BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(PIN_LED_WARNING, ledState);
      lastBlink = millis();
    }
  } else {
    digitalWrite(PIN_LED_WARNING, LOW);
  }

  status.roofState = servoGetState();
  status.roofAngle = servoGetCurrentAngle();
  status.overheating = overheating;

  return status;
}
```

### 8.6 smart_greenhouse.ino — Main File

```cpp
// smart_greenhouse.ino
// Smart Greenhouse IoT — Phase 1: Local Sensor + Servo
// Board: ESP32-WROVER-E DevKit

#include "config.h"
#include "sensors.h"
#include "servo_control.h"
#include "decision.h"

unsigned long lastSensorRead = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);  // Tunggu serial monitor ready

  Serial.println("========================================");
  Serial.println("  SMART GREENHOUSE IoT — Phase 1");
  Serial.println("  Mode: LOCAL ONLY (no WiFi)");
  Serial.println("========================================");

  // Inisialisasi LED warning
  pinMode(PIN_LED_WARNING, OUTPUT);
  digitalWrite(PIN_LED_WARNING, LOW);

  // Inisialisasi komponen
  sensorsInit();
  servoInit();
  decisionInit();

  Serial.println("[SYSTEM] Inisialisasi selesai. Mulai loop...\n");
}

void loop() {
  unsigned long now = millis();

  // Baca sensor setiap SENSOR_READ_INTERVAL (5 detik)
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;

    // Baca semua sensor
    SensorData sensorData = sensorsRead();

    // Proses keputusan
    SystemStatus status = decisionProcess(sensorData);

    // Print ke Serial Monitor (debugging)
    Serial.println("─────────────────────────────────────");
    Serial.printf("Suhu     : %.1f °C\n", status.sensors.temperature);
    Serial.printf("Humidity : %.1f %%\n", status.sensors.humidity);
    Serial.printf("Cahaya   : %.0f lux\n", status.sensors.lux);
    Serial.printf("Hujan    : %s\n", status.sensors.isRaining ? "YA" : "TIDAK");
    Serial.printf("Atap     : %s (%d°)\n",
      (status.roofState == ROOF_OPEN) ? "BUKA" :
      (status.roofState == ROOF_CLOSED) ? "TUTUP" : "TRANSISI",
      status.roofAngle);
    Serial.printf("Overheat : %s\n", status.overheating ? "WARNING!" : "Normal");
    Serial.printf("Mode     : %s\n", (status.mode == MODE_AUTO) ? "AUTO" : "MANUAL");
    Serial.println();
  }

  // Tetap update servo di setiap loop (untuk hysteresis timer)
  servoUpdate();
}
```

### 8.7 Cara Test Phase 1

```
1. Upload code ke ESP32 via Arduino IDE
2. Buka Serial Monitor (115200 baud)
3. Lihat output sensor setiap 5 detik

TEST CASES:
┌──────────────────────────────────────────────────────────────────┐
│ Test                        │ Cara                │ Expected     │
├─────────────────────────────┼─────────────────────┼──────────────┤
│ Rain = tutup atap           │ Teteskan air ke     │ Servo → 0°   │
│                             │ rain plate          │ setelah 60s  │
│                             │                     │              │
│ Cerah = buka atap           │ Arahkan lampu terang│ Servo → 180° │
│                             │ ke BH1750           │ setelah 60s  │
│                             │                     │              │
│ Anti-oven warning           │ Teteskan air (hujan)│ LED kedip    │
│                             │ + panaskan DHT11    │              │
│                             │ (hairdryer)         │              │
│                             │                     │              │
│ Hysteresis                  │ Teteskan air sebentar│ Servo TIDAK  │
│                             │ lalu keringkan <60s │ bergerak     │
│                             │                     │              │
│ Sensor error                │ Cabut kabel DHT11   │ sensorError  │
│                             │                     │ = true       │
└──────────────────────────────────────────────────────────────────┘
```

---

## 9. PHASE 2 — WIFI + SUPABASE INTEGRATION

> Tujuan: ESP32 terhubung ke WiFi, kirim data sensor ke Supabase,
> dan terima perintah manual dari user via Supabase.

### 9.1 File Baru

```
smart_greenhouse/
├── ... (file Phase 1)
├── wifi_manager.h / .cpp     // Koneksi WiFi + failsafe
├── supabase_client.h / .cpp  // HTTP client ke Supabase REST API
└── credentials.h             // SSID, password, API keys (JANGAN commit ke git)
```

### 9.2 credentials.h

```cpp
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// WiFi
#define WIFI_SSID     "nama_wifi_kamu"
#define WIFI_PASSWORD "password_wifi_kamu"

// Supabase
#define SUPABASE_URL  "https://xxxxx.supabase.co"
#define SUPABASE_KEY  "eyJhbGciOiJ..."  // anon public key

#endif
```

### 9.3 wifi_manager.h / .cpp

```cpp
// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

void wifiInit();
bool wifiIsConnected();
void wifiReconnectIfNeeded();  // Non-blocking, panggil di loop()

#endif
```

```cpp
// wifi_manager.cpp
#include "wifi_manager.h"
#include "config.h"
#include "credentials.h"
#include <WiFi.h>

unsigned long lastReconnectAttempt = 0;

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WIFI] Menghubungkan");

  // Tunggu max 10 detik untuk koneksi pertama
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Terhubung! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WIFI] Gagal terhubung — lanjut mode offline");
  }
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifiReconnectIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
    lastReconnectAttempt = now;
    Serial.println("[WIFI] Mencoba reconnect...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}
```

### 9.4 supabase_client.h / .cpp

```cpp
// supabase_client.h
#ifndef SUPABASE_CLIENT_H
#define SUPABASE_CLIENT_H

#include "sensors.h"
#include "decision.h"

// Kirim data sensor ke tabel "sensor_logs"
bool supabasePostSensorData(SystemStatus status);

// Cek perintah manual dari tabel "commands"
// Return: -1 = tidak ada perintah, 0 = tutup, 180 = buka, -2 = mode auto
int supabaseCheckCommand();

// Tandai perintah sudah dieksekusi
bool supabaseMarkCommandDone(int commandId);

#endif
```

```cpp
// supabase_client.cpp
#include "supabase_client.h"
#include "config.h"
#include "credentials.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Satu HTTPClient object, di-reuse
HTTPClient http;

bool supabasePostSensorData(SystemStatus status) {
  String url = String(SUPABASE_URL) + "/rest/v1/sensor_logs";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(5000);  // Timeout 5 detik

  // Buat JSON payload
  JsonDocument doc;
  doc["temperature"] = status.sensors.temperature;
  doc["humidity"] = status.sensors.humidity;
  doc["lux"] = status.sensors.lux;
  doc["is_raining"] = status.sensors.isRaining;
  doc["roof_angle"] = status.roofAngle;
  doc["roof_state"] = (status.roofState == ROOF_OPEN) ? "open" : "closed";
  doc["mode"] = (status.mode == MODE_AUTO) ? "auto" : "manual";
  doc["overheating"] = status.overheating;

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  http.end();

  if (httpCode == 201) {
    Serial.println("[SUPABASE] Data sensor terkirim");
    return true;
  } else {
    Serial.printf("[SUPABASE] Gagal kirim data, HTTP %d\n", httpCode);
    return false;
  }
}

int supabaseCheckCommand() {
  // Ambil perintah terbaru yang belum dieksekusi
  String url = String(SUPABASE_URL) +
    "/rest/v1/commands?executed=eq.false&order=created_at.desc&limit=1";

  http.begin(url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(5000);

  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    return -1;
  }

  String response = http.getString();
  http.end();

  JsonDocument doc;
  deserializeJson(doc, response);

  if (doc.size() == 0) return -1;  // Tidak ada perintah

  String action = doc[0]["action"].as<String>();
  int commandId = doc[0]["id"].as<int>();

  // Tandai sudah dieksekusi
  supabaseMarkCommandDone(commandId);

  if (action == "open") return SERVO_OPEN;
  if (action == "close") return SERVO_CLOSED;
  if (action == "auto") return -2;  // Kembali ke mode auto

  return -1;
}

bool supabaseMarkCommandDone(int commandId) {
  String url = String(SUPABASE_URL) +
    "/rest/v1/commands?id=eq." + String(commandId);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.setTimeout(5000);

  int httpCode = http.PATCH("{\"executed\": true}");
  http.end();

  return httpCode == 200 || httpCode == 204;
}
```

### 9.5 Update smart_greenhouse.ino untuk Phase 2

```cpp
// Tambahkan di bagian atas
#include "wifi_manager.h"
#include "supabase_client.h"

unsigned long lastSupabasePost = 0;
unsigned long lastCommandCheck = 0;

void setup() {
  // ... (setup Phase 1 tetap sama)

  // Tambahan Phase 2
  wifiInit();
}

void loop() {
  unsigned long now = millis();

  // WiFi failsafe — non-blocking reconnect
  wifiReconnectIfNeeded();

  // ===== SENSOR READ (tetap sama) =====
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;

    SensorData sensorData = sensorsRead();
    SystemStatus status = decisionProcess(sensorData);

    // Print ke serial (tetap sama)
    // ...

    // ===== PHASE 2: Kirim data ke Supabase =====
    if (wifiIsConnected() && (now - lastSupabasePost >= SUPABASE_POST_INTERVAL)) {
      lastSupabasePost = now;
      supabasePostSensorData(status);
    }
  }

  // ===== PHASE 2: Cek perintah manual dari Supabase =====
  if (wifiIsConnected() && (now - lastCommandCheck >= SUPABASE_CHECK_INTERVAL)) {
    lastCommandCheck = now;

    int command = supabaseCheckCommand();
    if (command == SERVO_OPEN || command == SERVO_CLOSED) {
      // Mode manual — override servo
      // TAPI Rule 1 tetap berlaku: kalau hujan, TETAP TUTUP
      currentMode = MODE_MANUAL;
      servoSetTarget(command);
      Serial.printf("[MANUAL] Perintah: %s\n", command == SERVO_OPEN ? "BUKA" : "TUTUP");
    } else if (command == -2) {
      // Kembali ke auto
      currentMode = MODE_AUTO;
      Serial.println("[MANUAL] Kembali ke mode AUTO");
    }
  }

  servoUpdate();
}
```

---

## 10. PHASE 3 — WEB DASHBOARD (VERCEL)

> Tujuan: Dashboard web yang menampilkan data sensor real-time
> dan tombol kontrol manual. Deploy di Vercel.

### 10.1 Struktur Project

```
greenhouse-dashboard/
├── app/
│   ├── layout.tsx          // Root layout
│   ├── page.tsx            // Dashboard utama
│   ├── globals.css         // Tailwind styles
│   └── components/
│       ├── SensorCard.tsx      // Card individual sensor
│       ├── RoofStatus.tsx      // Status atap + visualisasi
│       ├── ControlPanel.tsx    // Tombol manual: Buka / Tutup / Auto
│       ├── SensorChart.tsx     // Grafik historis sensor
│       └── WeatherForecast.tsx // Info cuaca dari Open-Meteo
├── lib/
│   └── supabase.ts         // Supabase client singleton
├── .env.local              // Environment variables
├── package.json
└── next.config.js
```

### 10.2 lib/supabase.ts

```typescript
import { createClient } from '@supabase/supabase-js'

const supabaseUrl = process.env.NEXT_PUBLIC_SUPABASE_URL!
const supabaseKey = process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!

export const supabase = createClient(supabaseUrl, supabaseKey)
```

### 10.3 Fitur Dashboard

```
┌─────────────────────────────────────────────────────────────────┐
│                  SMART GREENHOUSE DASHBOARD                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐      │
│  │  SUHU     │ │ HUMIDITY  │ │  CAHAYA   │ │  HUJAN    │      │
│  │  28.5°C   │ │  72.3%    │ │ 15420 lux │ │  TIDAK    │      │
│  │  ✅ Normal │ │  ✅ Normal │ │  ☀️ Terik  │ │  ✅ Cerah  │      │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘      │
│                                                                 │
│  ┌─────────────────────────────────────────┐                   │
│  │         STATUS ATAP: TERBUKA (180°)     │                   │
│  │         Mode: OTOMATIS                  │                   │
│  │         ┌──────┐ ┌──────┐ ┌──────┐     │                   │
│  │         │ BUKA │ │TUTUP │ │ AUTO │     │                   │
│  │         └──────┘ └──────┘ └──────┘     │                   │
│  └─────────────────────────────────────────┘                   │
│                                                                 │
│  ┌─────────────────────────────────────────┐                   │
│  │  GRAFIK HISTORIS (24 jam terakhir)      │                   │
│  │  📈 Suhu | Kelembapan | Cahaya          │                   │
│  └─────────────────────────────────────────┘                   │
│                                                                 │
│  ┌─────────────────────────────────────────┐                   │
│  │  PREDIKSI CUACA (6 jam ke depan)        │                   │
│  │  dari Open-Meteo API                    │                   │
│  └─────────────────────────────────────────┘                   │
│                                                                 │
│  ⚠️ ALERT: Overheating detected!            (jika aktif)       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 10.4 Cara Kontrol Manual Bekerja

```
USER klik "BUKA" di dashboard
    │
    ▼
Frontend INSERT ke Supabase tabel "commands":
    { action: "open", executed: false }
    │
    ▼
ESP32 GET /commands?executed=eq.false (tiap 10 detik)
    │
    ▼
ESP32 baca action "open" → set servo target 180°
    │
    ▼
ESP32 PATCH /commands?id=eq.X → { executed: true }
    │
    ▼
Servo bergerak (setelah hysteresis 60 detik)
    │
    ▼
ESP32 POST /sensor_logs → { roof_state: "open", mode: "manual" }
    │
    ▼
Dashboard auto-refresh → tampilkan status baru
```

### 10.5 Real-time Update

```typescript
// Supabase Realtime subscription di dashboard
// Data update otomatis tanpa perlu refresh halaman

const channel = supabase
  .channel('sensor-updates')
  .on(
    'postgres_changes',
    { event: 'INSERT', schema: 'public', table: 'sensor_logs' },
    (payload) => {
      // Update state dashboard dengan data terbaru
      setSensorData(payload.new)
    }
  )
  .subscribe()
```

### 10.6 Deploy ke Vercel

```bash
# Install Vercel CLI
npm i -g vercel

# Deploy
vercel

# Set environment variables di Vercel dashboard:
# NEXT_PUBLIC_SUPABASE_URL = https://xxxxx.supabase.co
# NEXT_PUBLIC_SUPABASE_ANON_KEY = eyJhbGciOiJ...
```

---

## 11. PHASE 4 — OPEN-METEO API INTEGRATION

> Tujuan: ESP32 ambil prediksi cuaca dari Open-Meteo API
> untuk antisipasi hujan sebelum terjadi.

### 11.1 Tentang Open-Meteo

```
- API gratis, tanpa API key, tanpa registrasi
- Rate limit: 10.000 request/hari (kita cuma ~48/hari = sangat aman)
- Forecast hingga 7 hari ke depan
- Data: suhu, curah hujan, probabilitas hujan, kecepatan angin, dll
```

### 11.2 Endpoint yang Dipakai

```
GET https://api.open-meteo.com/v1/forecast
  ?latitude=-6.8915          // Koordinat Bandung (ITB)
  ?longitude=107.6107
  &hourly=temperature_2m,precipitation_probability,weathercode
  &forecast_days=1
  &timezone=Asia/Jakarta
```

### 11.3 Response (contoh)

```json
{
  "hourly": {
    "time": ["2026-04-16T00:00", "2026-04-16T01:00", ...],
    "temperature_2m": [24.1, 23.8, ...],
    "precipitation_probability": [10, 15, 20, 60, 80, ...],
    "weathercode": [0, 1, 2, 61, 63, ...]
  }
}
```

### 11.4 Firmware — weather_client.h / .cpp

```cpp
// weather_client.h
#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

struct WeatherForecast {
  bool willRain;                  // Prediksi hujan dalam 6 jam ke depan
  int rainProbability;            // Probabilitas tertinggi (%)
  float forecastTemp;             // Suhu prediksi rata-rata
  bool fetchSuccess;              // Apakah berhasil ambil data
};

WeatherForecast weatherFetch();

#endif
```

```cpp
// weather_client.cpp
#include "weather_client.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Koordinat Bandung (ITB)
#define LATITUDE  "-6.8915"
#define LONGITUDE "107.6107"

WeatherForecast weatherFetch() {
  WeatherForecast forecast;
  forecast.fetchSuccess = false;
  forecast.willRain = false;
  forecast.rainProbability = 0;
  forecast.forecastTemp = 0;

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast"
    "?latitude=" LATITUDE
    "&longitude=" LONGITUDE
    "&hourly=temperature_2m,precipitation_probability,weathercode"
    "&forecast_days=1"
    "&timezone=Asia/Jakarta";

  http.begin(url);
  http.setTimeout(10000);  // Timeout 10 detik (API eksternal bisa lambat)

  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("[WEATHER] Gagal fetch, HTTP %d\n", httpCode);
    http.end();
    return forecast;
  }

  String response = http.getString();
  http.end();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.printf("[WEATHER] JSON parse error: %s\n", error.c_str());
    return forecast;
  }

  // Cek 6 jam ke depan
  // Ambil jam sekarang, lalu cek index 0-5
  JsonArray probabilities = doc["hourly"]["precipitation_probability"];
  JsonArray temperatures = doc["hourly"]["temperature_2m"];
  JsonArray weathercodes = doc["hourly"]["weathercode"];

  int maxProb = 0;
  float avgTemp = 0;
  int count = min((int)probabilities.size(), 6);  // 6 jam ke depan

  for (int i = 0; i < count; i++) {
    int prob = probabilities[i].as<int>();
    if (prob > maxProb) maxProb = prob;
    avgTemp += temperatures[i].as<float>();
  }

  avgTemp /= count;

  forecast.rainProbability = maxProb;
  forecast.willRain = (maxProb >= 60);  // >=60% = kemungkinan besar hujan
  forecast.forecastTemp = avgTemp;
  forecast.fetchSuccess = true;

  Serial.printf("[WEATHER] Prediksi: hujan=%s (prob=%d%%), suhu=%.1f°C\n",
    forecast.willRain ? "YA" : "TIDAK",
    forecast.rainProbability,
    forecast.forecastTemp
  );

  return forecast;
}
```

### 11.5 Integrasi ke Decision Engine

```cpp
// Tambahan di decision.cpp

WeatherForecast lastForecast;

SystemStatus decisionProcess(SensorData data) {
  // ... (Rule 1 tetap sama — rain sensor override semua)

  // Kalau tidak hujan secara fisik:
  if (!data.isRaining) {

    // Cek prediksi cuaca
    if (lastForecast.fetchSuccess && lastForecast.willRain) {
      // Prediksi hujan ≥60% → tutup atap sebagai antisipasi
      servoSetTarget(SERVO_CLOSED);
      Serial.println("[DECISION] Tutup atap — prediksi hujan");
    }
    else {
      // Keputusan normal berdasarkan sensor lokal
      // ... (logika lux dan suhu seperti sebelumnya)
    }
  }
}
```

### 11.6 Prioritas Keputusan (Urutan)

```
PRIORITAS 1 (tertinggi): Rain Sensor fisik = HUJAN
    → TUTUP ATAP. Tidak bisa di-override.

PRIORITAS 2: Perintah manual dari user
    → Eksekusi perintah. KECUALI kalau hujan (Rule 1).

PRIORITAS 3: Prediksi cuaca Open-Meteo
    → Kalau prediksi hujan ≥60%, tutup atap antisipasi.

PRIORITAS 4 (terendah): Logika sensor lokal
    → Cahaya terang + suhu OK = buka
    → Gelap/mendung = tutup
```

---

## 12. DATABASE SCHEMA (SUPABASE)

### 12.1 Tabel: sensor_logs

```sql
CREATE TABLE sensor_logs (
  id            BIGSERIAL PRIMARY KEY,
  temperature   FLOAT,                              -- °C dari DHT11
  humidity      FLOAT,                              -- % dari DHT11
  lux           FLOAT,                              -- lux dari BH1750
  is_raining    BOOLEAN DEFAULT FALSE,              -- dari rain sensor
  roof_angle    INTEGER DEFAULT 0,                  -- 0-180 derajat
  roof_state    TEXT DEFAULT 'closed',              -- 'open' atau 'closed'
  mode          TEXT DEFAULT 'auto',                -- 'auto' atau 'manual'
  overheating   BOOLEAN DEFAULT FALSE,              -- anti-oven flag
  created_at    TIMESTAMPTZ DEFAULT NOW()           -- timestamp otomatis
);

-- Index untuk query dashboard (data terbaru)
CREATE INDEX idx_sensor_logs_created_at ON sensor_logs (created_at DESC);

-- Aktifkan Realtime untuk tabel ini
ALTER PUBLICATION supabase_realtime ADD TABLE sensor_logs;
```

### 12.2 Tabel: commands

```sql
CREATE TABLE commands (
  id            BIGSERIAL PRIMARY KEY,
  action        TEXT NOT NULL,                       -- 'open', 'close', 'auto'
  executed      BOOLEAN DEFAULT FALSE,               -- sudah dieksekusi ESP32?
  created_at    TIMESTAMPTZ DEFAULT NOW(),
  executed_at   TIMESTAMPTZ                          -- kapan dieksekusi
);

-- Index untuk query ESP32 (perintah belum dieksekusi)
CREATE INDEX idx_commands_pending ON commands (executed, created_at DESC)
  WHERE executed = FALSE;
```

### 12.3 Tabel: weather_cache (opsional)

```sql
CREATE TABLE weather_cache (
  id                BIGSERIAL PRIMARY KEY,
  will_rain         BOOLEAN,
  rain_probability  INTEGER,                         -- %
  forecast_temp     FLOAT,                           -- °C
  fetched_at        TIMESTAMPTZ DEFAULT NOW()
);
```

### 12.4 Row Level Security (RLS)

```sql
-- Aktifkan RLS
ALTER TABLE sensor_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE commands ENABLE ROW LEVEL SECURITY;

-- Policy: anon bisa baca sensor_logs (untuk dashboard)
CREATE POLICY "Allow read sensor_logs" ON sensor_logs
  FOR SELECT USING (true);

-- Policy: anon bisa insert sensor_logs (dari ESP32)
CREATE POLICY "Allow insert sensor_logs" ON sensor_logs
  FOR INSERT WITH CHECK (true);

-- Policy: anon bisa CRUD commands (dashboard insert, ESP32 update)
CREATE POLICY "Allow all commands" ON commands
  FOR ALL USING (true) WITH CHECK (true);
```

### 12.5 Auto-Cleanup (Opsional)

```sql
-- Hapus data sensor_logs yang lebih dari 30 hari
-- Jalankan via Supabase Edge Function + cron, atau manual
DELETE FROM sensor_logs WHERE created_at < NOW() - INTERVAL '30 days';
```

---

## 13. API ENDPOINT REFERENCE

### ESP32 → Supabase (REST API)

| Method | Endpoint | Body | Fungsi |
|--------|----------|------|--------|
| POST | `/rest/v1/sensor_logs` | `{temperature, humidity, lux, ...}` | Kirim data sensor |
| GET | `/rest/v1/commands?executed=eq.false&order=created_at.desc&limit=1` | - | Cek perintah manual |
| PATCH | `/rest/v1/commands?id=eq.{id}` | `{executed: true}` | Tandai perintah selesai |

### ESP32 → Open-Meteo

| Method | Endpoint | Fungsi |
|--------|----------|--------|
| GET | `api.open-meteo.com/v1/forecast?latitude=...&longitude=...` | Ambil prediksi cuaca |

### Dashboard → Supabase (via supabase-js)

| Method | Tabel | Fungsi |
|--------|-------|--------|
| SELECT | `sensor_logs` (order by created_at desc, limit 1) | Data sensor terbaru |
| SELECT | `sensor_logs` (last 24 hours) | Data historis untuk grafik |
| INSERT | `commands` | Kirim perintah manual |
| REALTIME | `sensor_logs` (on INSERT) | Live update dashboard |

### Headers yang Wajib (ESP32 HTTP)

```
Content-Type: application/json
apikey: {SUPABASE_ANON_KEY}
Authorization: Bearer {SUPABASE_ANON_KEY}
```

---

## 14. STATE MACHINE & DECISION TREE

### 14.1 State Machine Atap

```
                    ┌──────────────────┐
                    │                  │
         ┌─────────▼──────────┐       │
    ┌───►│   ROOF_CLOSED (0°) │       │
    │    └─────────┬──────────┘       │
    │              │                  │
    │    Kondisi buka terpenuhi       │
    │    + stabil 60 detik            │
    │              │                  │
    │    ┌─────────▼──────────┐       │
    │    │  ROOF_TRANSITIONING │       │
    │    │  (menunggu 60 detik)│       │
    │    └─────────┬──────────┘       │
    │              │                  │
    │    Kondisi berubah       60 detik tercapai
    │    sebelum 60 detik             │
    │              │           ┌──────▼───────────┐
    │              │           │  ROOF_OPEN (180°) │
    │              └───────────►                   │
    │                          └──────┬───────────┘
    │                                 │
    │    Hujan / perintah tutup       │
    │    + stabil 60 detik            │
    └─────────────────────────────────┘
```

### 14.2 Decision Tree Lengkap

```
MULAI
│
├── Rain Sensor = HUJAN?
│   ├── YA → TUTUP ATAP
│   │        ├── Suhu > 33°C?
│   │        │   ├── YA → LED WARNING ON + flag overheating
│   │        │   └── TIDAK → Normal, tunggu hujan berhenti
│   │        └── (tidak bisa di-override, bahkan manual)
│   │
│   └── TIDAK (tidak hujan secara fisik)
│       │
│       ├── Ada perintah MANUAL dari Supabase?
│       │   ├── YA, action = "open"  → BUKA ATAP
│       │   ├── YA, action = "close" → TUTUP ATAP
│       │   ├── YA, action = "auto"  → Kembali ke mode AUTO
│       │   └── TIDAK (tidak ada perintah / mode AUTO)
│       │       │
│       │       ├── Prediksi Open-Meteo: hujan ≥60%?
│       │       │   ├── YA → TUTUP ATAP (antisipasi)
│       │       │   └── TIDAK
│       │       │       │
│       │       │       ├── Lux > 10000 DAN Suhu < 33°C?
│       │       │       │   └── YA → BUKA ATAP (cerah, ideal)
│       │       │       │
│       │       │       ├── Lux 500-10000?
│       │       │       │   └── BUKA ATAP (cahaya cukup)
│       │       │       │
│       │       │       └── Lux < 500?
│       │       │           └── TUTUP ATAP (gelap/malam)
│       │       │
│       │       └── WiFi mati? → Skip Supabase & Open-Meteo
│       │                        → Pakai sensor lokal saja
│       │
│       └── Semua keputusan di atas → masuk HYSTERESIS
│           └── Tunggu 60 detik stabil → baru gerak servo
```

---

## 15. PENANGANAN ERROR & EDGE CASES

### 15.1 Tabel Error Handling

| Situasi | Deteksi | Response | Prioritas |
|---------|---------|----------|-----------|
| DHT11 gagal baca | `isnan(temperature)` | Gunakan data terakhir yang valid. Log error. Jangan gerak servo berdasarkan data NaN. | Tinggi |
| BH1750 gagal baca | `lux < 0` | Gunakan data terakhir. Jangan tutup atap karena "gelap" palsu. | Tinggi |
| Rain sensor kabel putus | `digitalRead` selalu HIGH | Diasumsikan tidak hujan. Ini acceptable — lebih bahaya kalau false-positive rain. | Sedang |
| WiFi disconnect | `WiFi.status() != WL_CONNECTED` | Skip semua HTTP. Lanjut mode lokal. Coba reconnect tiap 30 detik. | Tinggi |
| Supabase POST gagal | `httpCode != 201` | Log error, skip. Coba lagi di interval berikutnya. Data tidak di-buffer (RAM terbatas). | Rendah |
| Supabase GET gagal | `httpCode != 200` | Log error, skip. Mode auto pakai sensor lokal. | Rendah |
| Open-Meteo timeout | `httpCode != 200` | `fetchSuccess = false`. Keputusan hanya berdasarkan sensor lokal. | Rendah |
| Servo stall/macet | Arus spike, servo panas | Limit sudut servo di firmware. Capacitor buffer lonjakan. | Sedang |
| Semua sensor gagal | Semua data = error | JANGAN gerak servo. Tetap di posisi terakhir. Nyalakan LED warning. | Kritis |
| Power mati lalu nyala | ESP32 restart | `setup()` → servo ke posisi TUTUP (aman). Baca sensor. Resume normal. | Tinggi |
| millis() overflow | Setelah ~49 hari | Gunakan `unsigned long` + aritmetika unsigned. Otomatis handle overflow. | Rendah |

### 15.2 Prinsip Failsafe

```
JIKA RAGU → TUTUP ATAP

Lebih baik tanaman kurang sinar 1 jam
daripada kehujanan karena sensor error.
```

---

## 16. TESTING & VALIDASI

### 16.1 Phase 1 Test Plan

| # | Test Case | Cara Test | Expected Result | Pass? |
|---|-----------|-----------|-----------------|-------|
| 1.1 | Sensor baca normal | Power on, buka Serial Monitor | Suhu, humidity, lux, rain muncul tiap 5 detik | [ ] |
| 1.2 | Rain → tutup atap | Teteskan air ke rain plate | Servo ke 0° setelah 60s hysteresis | [ ] |
| 1.3 | Cerah → buka atap | Lampu terang ke BH1750 | Servo ke 180° setelah 60s | [ ] |
| 1.4 | Hysteresis bekerja | Teteskan air <60s lalu keringkan | Servo TIDAK bergerak (timer reset) | [ ] |
| 1.5 | Anti-oven | Air di rain plate + panaskan DHT11 >33°C | LED merah berkedip | [ ] |
| 1.6 | Sensor error DHT11 | Cabut kabel DHT11 | Serial: error message, servo tidak bergerak | [ ] |
| 1.7 | Sensor error BH1750 | Cabut kabel SDA | Serial: error message, servo tidak bergerak | [ ] |
| 1.8 | Power cycle | Cabut-colok adaptor | Servo kembali ke posisi TUTUP | [ ] |

### 16.2 Phase 2 Test Plan

| # | Test Case | Cara Test | Expected Result | Pass? |
|---|-----------|-----------|-----------------|-------|
| 2.1 | WiFi connect | Power on dengan WiFi tersedia | Serial: "Terhubung! IP: ..." | [ ] |
| 2.2 | Data ke Supabase | Tunggu 60 detik | Cek Supabase dashboard: row baru muncul | [ ] |
| 2.3 | Perintah manual open | Insert command via Supabase SQL editor | Servo buka setelah 60s | [ ] |
| 2.4 | Perintah manual close | Insert command via Supabase SQL editor | Servo tutup setelah 60s | [ ] |
| 2.5 | Manual override vs hujan | Kirim "open" tapi rain sensor aktif | Servo TETAP TUTUP (Rule 1) | [ ] |
| 2.6 | WiFi disconnect | Matikan router WiFi | Servo tetap jalan berdasarkan sensor lokal | [ ] |
| 2.7 | WiFi reconnect | Nyalakan router kembali | Auto reconnect, data kembali terkirim | [ ] |

### 16.3 Phase 3 Test Plan

| # | Test Case | Cara Test | Expected Result | Pass? |
|---|-----------|-----------|-----------------|-------|
| 3.1 | Dashboard load | Buka URL Vercel | Sensor cards tampil dengan data terbaru | [ ] |
| 3.2 | Real-time update | Tunggu ESP32 kirim data | Dashboard update tanpa refresh | [ ] |
| 3.3 | Tombol BUKA | Klik tombol BUKA di dashboard | Command masuk Supabase, ESP32 eksekusi | [ ] |
| 3.4 | Tombol TUTUP | Klik tombol TUTUP | Servo tutup | [ ] |
| 3.5 | Tombol AUTO | Klik tombol AUTO | Kembali ke mode otomatis | [ ] |
| 3.6 | Grafik historis | Biarkan jalan 1+ jam | Grafik suhu/humidity/lux muncul | [ ] |
| 3.7 | Alert overheating | Trigger anti-oven di ESP32 | Banner peringatan muncul di dashboard | [ ] |

### 16.4 Phase 4 Test Plan

| # | Test Case | Cara Test | Expected Result | Pass? |
|---|-----------|-----------|-----------------|-------|
| 4.1 | Fetch forecast | Tunggu 30 menit atau restart | Serial: prediksi cuaca muncul | [ ] |
| 4.2 | Prediksi hujan | Cek saat cuaca mendung | willRain = true, atap tutup antisipasi | [ ] |
| 4.3 | API timeout | Blok api.open-meteo.com di router | fetchSuccess = false, lanjut sensor lokal | [ ] |

---

## 17. ESTIMASI KELISTRIKAN & POWER BUDGET

### 17.1 Konsumsi Tiap Komponen

| Komponen | Voltage | Arus Normal | Arus Peak | Sumber Power |
|----------|---------|-------------|-----------|--------------|
| ESP32-WROVER-E (WiFi aktif) | 5V via VIN | 150mA | 250mA | Adaptor via VIN |
| BH1750 (GY-302) | 3.3V | 0.12mA | 0.12mA | Pin 3.3V ESP32 |
| DHT11 Module | 3.3V | 0.5mA | 2.5mA | Pin 3.3V ESP32 |
| Rain Sensor Module | 3.3V | 15mA | 15mA | Pin 3.3V ESP32 |
| MG996R Servo (jalan) | 5V | 500mA | 900mA | Adaptor langsung |
| MG996R Servo (stall) | 5V | - | 2500mA | Adaptor langsung |
| LED Warning (2 buah) | 3.3V | 15mA×2 | 15mA×2 | GPIO via R220Ω |

### 17.2 Total Power Budget

```
Kondisi NORMAL (servo bergerak):
  ESP32      :  150 mA
  Sensor     :   18 mA  (BH1750 + DHT11 + Rain + LED)
  Servo      :  500 mA
  ─────────────────────
  Total      :  668 mA  ← adaptor 3A sangat cukup (22% load)

Kondisi PEAK (servo stall):
  ESP32      :  250 mA
  Sensor     :   48 mA
  Servo stall: 2500 mA
  ─────────────────────
  Total      : 2798 mA  ← adaptor 3A masih cukup (93% load)
                           capacitor 470µF bantu handle lonjakan sesaat

Kondisi IDLE (servo diam, WiFi sleep):
  ESP32      :   80 mA
  Sensor     :   16 mA
  Servo idle :   10 mA
  ─────────────────────
  Total      :  106 mA  ← sangat hemat
```

### 17.3 Pin 3.3V ESP32 — Cukup?

```
Pin 3.3V ESP32 max output: ~500mA (dari onboard regulator)

Beban di 3.3V:
  BH1750  :  0.12 mA
  DHT11   :  2.50 mA
  Rain    : 15.00 mA
  LED×2   : 30.00 mA
  ─────────────────────
  Total   : 47.62 mA   ← jauh di bawah 500mA limit. AMAN.
```

---

## 18. TIMELINE PENGERJAAN

### Asumsi: Komponen sudah di tangan, coding ~2-4 jam/hari

```
MINGGU 1: Phase 1 — Hardware + Firmware Lokal
├── Hari 1-2: Rakit hardware di breadboard, test wiring
├── Hari 3-4: Code sensor reading, test tiap sensor individual
├── Hari 5-6: Code servo control + hysteresis
└── Hari 7:   Code decision engine + test semua Rule 1-4

MINGGU 2: Phase 2 — WiFi + Supabase
├── Hari 1:   Setup Supabase project + buat tabel
├── Hari 2-3: Code WiFi manager + failsafe offline
├── Hari 4-5: Code Supabase client (POST sensor, GET commands)
└── Hari 6-7: Integration test + debug

MINGGU 3: Phase 3 — Web Dashboard
├── Hari 1-2: Setup Next.js + Supabase client
├── Hari 3-4: Build sensor cards + roof status
├── Hari 5:   Build control panel (tombol manual)
├── Hari 6:   Build grafik historis
└── Hari 7:   Deploy ke Vercel + test end-to-end

MINGGU 4: Phase 4 — Open-Meteo + Polish
├── Hari 1-2: Code weather client di firmware
├── Hari 3:   Integrasi ke decision engine
├── Hari 4:   Tampilkan forecast di dashboard
├── Hari 5-7: Full system test + bug fix + dokumentasi
```

---

## 19. TROUBLESHOOTING

| Problem | Kemungkinan Penyebab | Solusi |
|---------|---------------------|--------|
| ESP32 tidak muncul di COM port | Kabel USB charge-only | Ganti kabel USB yang support data |
| Upload gagal "Connecting..." | ESP32 tidak masuk boot mode | Tahan tombol BOOT, tekan EN, lepas BOOT setelah "Connecting..." muncul |
| DHT11 baca NaN | Kabel longgar / modul rusak | Cek kabel, coba pin lain, pastikan module (bukan bare sensor) |
| BH1750 baca 0 terus | I2C address salah / kabel SDA-SCL ketukar | Cek dengan I2C scanner sketch. SDA=GPIO21, SCL=GPIO22 |
| Rain sensor selalu LOW | Sensitivity terlalu tinggi | Putar potensiometer di modul sampai DO = HIGH saat kering |
| Servo tidak bergerak | Power kurang / sinyal salah | Cek servo power dari adaptor (bukan ESP32). Cek GPIO13. |
| Servo getar/jitter | Power tidak stabil | Pastikan capacitor 470µF terpasang. Cek common ground. |
| WiFi gagal connect | SSID/password salah / jarak jauh | Double-check credentials.h. Dekatkan ESP32 ke router. |
| Supabase 401 Unauthorized | API key salah | Cek SUPABASE_KEY di credentials.h = anon key (bukan service_role) |
| Supabase 404 | Nama tabel salah | Pastikan tabel `sensor_logs` dan `commands` sudah dibuat |
| Open-Meteo timeout | Internet lambat | Naikkan timeout ke 15000ms. Atau cek apakah WiFi aktif. |
| ESP32 restart sendiri | Watchdog timeout / memory leak | Pastikan tidak ada `delay()` panjang. Cek free heap: `ESP.getFreeHeap()` |
| Overheat ESP32 | Short circuit / beban berlebihan di pin | CABUT SEGERA. Cek wiring — jangan ada 5V masuk ke pin 3.3V |

---

## 20. REFERENSI DATASHEET

| Komponen | Datasheet/Referensi |
|----------|-------------------|
| ESP32-WROVER-E | Espressif ESP32-WROVER-E Datasheet |
| BH1750FVI | ROHM BH1750FVI Datasheet |
| DHT11 | Aosong DHT11 Datasheet |
| MG996R | TowerPro MG996R Datasheet |
| LM393 (Rain Module) | TI LM393 Comparator Datasheet |

Cari di Google dengan keyword: `"{nama komponen}" datasheet PDF`

---

## CATATAN AKHIR

```
Dokumen ini adalah panduan LENGKAP.
Ikuti dari atas ke bawah, phase by phase.
Jangan loncat phase — setiap phase bergantung pada phase sebelumnya.

Kalau stuck, cek bagian TROUBLESHOOTING (Section 19).
Kalau masih stuck, baca error message di Serial Monitor — itu clue terbaik.

Selamat membangun! 🌱
```
