# Smart Greenhouse IoT

Sistem greenhouse otomatis berbasis **ESP32-S3-N16R8** yang bisa buka/tutup atap louver berdasarkan sensor cuaca lokal, prediksi cuaca internet, dan perintah manual dari web dashboard.

## Arsitektur

```
[BH1750] [DHT11] [Rain Sensor]
     \      |      /
      ESP32-S3-N16R8 ──── [Servo MG996R] ──── Atap Louver
            |
          WiFi
         /    \
   Supabase   Open-Meteo API
       |
   Web Dashboard (Vercel)
```

**Prinsip utama:** ESP32 membaca sensor, mengambil keputusan, dan menggerakkan servo. Data dikirim ke Supabase. Web dashboard di Vercel hanya baca dari Supabase. User bisa kirim perintah manual via dashboard ke Supabase, yang kemudian di-poll oleh ESP32.

---

## Aturan Logika Kritis

### Rule 1: Local is King
Sensor hujan fisik mengoverride segalanya. Hujan terdeteksi = atap tutup. Tidak bisa di-override oleh perintah manual maupun prediksi cuaca.

### Rule 2: Hysteresis (Anti-Jitter)
Kondisi harus stabil selama **60 detik** sebelum servo bergerak. Mencegah atap buka-tutup-buka-tutup karena hujan rintik. Implementasi pakai `millis()`, bukan `delay()`.

### Rule 3: Failsafe Offline
WiFi putus? Sistem tetap jalan dengan sensor lokal. Tidak crash, tidak restart. Coba reconnect tiap 30 detik secara non-blocking.

### Rule 4: Anti-Oven Effect
Atap tertutup (hujan) tapi suhu > 33°C? LED warning berkedip + flag `overheating` dikirim ke dashboard. Atap tetap tidak dibuka karena masih hujan.

---

## Daftar Komponen

| No | Komponen | Model | Qty | Est. Harga |
|----|----------|-------|-----|------------|
| 1 | Microcontroller | ESP32-S3-N16R8 Soldered | 1 | 90-140rb |
| 2 | Sensor Cahaya | BH1750 GY-302 | 1 | 8-15rb |
| 3 | Sensor Suhu/Humidity | DHT11 Module 3-pin | 1 | 8-15rb |
| 4 | Sensor Hujan | MH-RD + LM393 | 1 set | 10-18rb |
| 5 | Servo Motor | MG996R Metal Gear | 1 | 35-55rb |
| 6 | Adaptor | DC 5V 3A (AC 220V input) | 1 | 20-35rb |
| 7 | Jack DC | Female 5.5mm Screw Terminal | 1 | 3-5rb |
| 8 | Kabel USB | USB-C Data (bukan charge-only) | 1 | 8-15rb |
| 9 | Jumper M-M | 20cm, 40pcs | 1 set | 8-12rb |
| 10 | Jumper M-F | 20cm, 40pcs | 1 set | 8-12rb |
| 11 | Breadboard | 830 tie points | 1 | 12-20rb |
| 12 | Capacitor | 470µF 16V Elektrolit | 1 | 2-5rb |
| 13 | LED Merah | 5mm | 2 | 1-2rb |
| 14 | Resistor | 220 ohm 1/4W | 2 | 0.5-1rb |
| 15 | Terminal Block | 2-pin PCB (opsional) | 2 | 3-5rb |

**Total estimasi: Rp 216.500 - 355.000**

---

## Wiring

```
ADAPTOR 5V 3A (colok ke stopkontak 220V)
│
JACK DC FEMALE SCREW TERMINAL
├── +5V ─────┬──→ ESP32 VIN
│            ├──→ Servo VCC (merah) ← pasang capacitor 470µF di sini
│            │    kaki (+) ke +5V, kaki (-) ke GND
│
├── GND ─────┬──→ ESP32 GND
│            ├──→ Servo GND (coklat)
│            ├──→ BH1750 GND
│            ├──→ DHT11 GND
│            ├──→ Rain Module GND
│            └──→ LED Katoda (-)

ESP32 3.3V ──┬──→ BH1750 VCC
             ├──→ DHT11 VCC
             └──→ Rain Module VCC

ESP32 GPIO8  (SDA) ──→ BH1750 SDA
ESP32 GPIO9  (SCL) ──→ BH1750 SCL
ESP32 GPIO4        ──→ DHT11 DATA
ESP32 GPIO5        ──→ Rain Sensor DO (JANGAN sambung AO)
ESP32 GPIO13       ──→ Servo Signal (oranye)
ESP32 GPIO2        ──→ Resistor 220Ω ──→ LED Anoda (+) ──→ GND
```

### Aturan Wiring Kritis

- **Semua sensor pakai 3.3V** dari pin 3.3V ESP32
- **Servo power dari adaptor 5V langsung**, BUKAN dari ESP32
- **Rain sensor: pakai DO saja**, AO jangan disambung (bisa 5V, rusak ESP32)
- **Common ground wajib**: semua GND harus terhubung
- **Capacitor 470µF**: kaki (+) ke 5V, kaki (-) ke GND, sedekat mungkin dengan servo

---

## Setup Development Environment

### Arduino IDE

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)

2. Tambahkan ESP32 board:
   ```
   File → Preferences → Additional Board Manager URLs:
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. Install board:
   ```
   Tools → Board → Board Manager → cari "esp32" → Install "esp32 by Espressif"
   ```

4. **Board settings (PENTING untuk ESP32-S3-N16R8):**
   ```
   Board          : ESP32S3 Dev Module
   PSRAM          : OPI PSRAM         ← WAJIB OPI, bukan QSPI
   Flash Size     : 16MB (128Mb)
   USB CDC On Boot: Enabled           ← supaya Serial Monitor jalan via USB
   Upload Speed   : 921600
   ```

5. Install library (Tools → Manage Libraries):
   - `BH1750` by Christopher Laws
   - `DHT sensor library` by Adafruit
   - `Adafruit Unified Sensor` by Adafruit
   - `ESP32Servo` by Kevin Harrington
   - `ArduinoJson` by Benoit Blanchon

### Supabase

1. Buat akun di [supabase.com](https://supabase.com)
2. New Project → nama: `smart-greenhouse`, region: Singapore
3. Catat **Project URL** dan **Anon Key** (Settings → API)
4. Jalankan SQL di bawah untuk membuat tabel

### Credentials

```bash
cd firmware/
cp credentials.h.example credentials.h
# Edit credentials.h → isi WiFi dan Supabase credentials
```

---

## Database Schema (Supabase)

Jalankan SQL ini di Supabase SQL Editor (Dashboard → SQL Editor → New Query):

```sql
-- =============================================
-- Tabel 1: sensor_logs
-- Menyimpan data sensor dari ESP32 tiap 60 detik
-- =============================================
CREATE TABLE sensor_logs (
  id            BIGSERIAL PRIMARY KEY,
  temperature   FLOAT,
  humidity      FLOAT,
  lux           FLOAT,
  is_raining    BOOLEAN DEFAULT FALSE,
  roof_angle    INTEGER DEFAULT 0,
  roof_state    TEXT DEFAULT 'closed',
  mode          TEXT DEFAULT 'auto',
  overheating   BOOLEAN DEFAULT FALSE,
  created_at    TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_sensor_logs_created_at
  ON sensor_logs (created_at DESC);

ALTER PUBLICATION supabase_realtime
  ADD TABLE sensor_logs;

-- =============================================
-- Tabel 2: commands
-- Perintah manual dari user via web dashboard
-- =============================================
CREATE TABLE commands (
  id            BIGSERIAL PRIMARY KEY,
  action        TEXT NOT NULL,           -- 'open', 'close', 'auto'
  executed      BOOLEAN DEFAULT FALSE,
  created_at    TIMESTAMPTZ DEFAULT NOW(),
  executed_at   TIMESTAMPTZ
);

CREATE INDEX idx_commands_pending
  ON commands (executed, created_at DESC)
  WHERE executed = FALSE;

-- =============================================
-- Tabel 3: weather_cache (opsional)
-- Cache data prediksi cuaca dari Open-Meteo
-- =============================================
CREATE TABLE weather_cache (
  id                BIGSERIAL PRIMARY KEY,
  will_rain         BOOLEAN,
  rain_probability  INTEGER,
  forecast_temp     FLOAT,
  fetched_at        TIMESTAMPTZ DEFAULT NOW()
);

-- =============================================
-- Row Level Security (RLS)
-- =============================================
ALTER TABLE sensor_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE commands ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Allow read sensor_logs"
  ON sensor_logs FOR SELECT USING (true);

CREATE POLICY "Allow insert sensor_logs"
  ON sensor_logs FOR INSERT WITH CHECK (true);

CREATE POLICY "Allow all commands"
  ON commands FOR ALL USING (true) WITH CHECK (true);
```

---

## Struktur File Firmware

```
firmware/
├── smart_greenhouse.ino      ← File utama: setup() dan loop()
├── config.h                  ← Pin, threshold, timing (EDIT sesuai kebutuhan)
├── credentials.h.example     ← Template credentials (COPY jadi credentials.h)
├── sensors.h                 ← Header sensor
├── sensors.cpp               ← Baca BH1750, DHT11, Rain Sensor
├── servo_control.h           ← Header servo
├── servo_control.cpp         ← Kontrol servo + hysteresis 60 detik
├── decision.h                ← Header decision engine
├── decision.cpp              ← Otak sistem: Rule 1-4, prioritas keputusan
├── wifi_manager.h            ← Header WiFi
├── wifi_manager.cpp          ← Koneksi WiFi + failsafe offline
├── supabase_client.h         ← Header Supabase
├── supabase_client.cpp       ← POST data, GET perintah, PATCH status
├── weather_client.h          ← Header Open-Meteo
└── weather_client.cpp        ← Fetch prediksi cuaca
```

---

## Cara Upload Firmware

1. Buka `firmware/smart_greenhouse.ino` di Arduino IDE
2. Pastikan board settings sudah benar (lihat bagian Setup di atas)
3. Colok ESP32-S3 ke PC via kabel USB-C **data**
4. Pilih port yang muncul (Tools → Port)
5. Klik Upload

**Kalau upload gagal "Connecting...":**
1. Tahan tombol **BOOT** di ESP32
2. Tekan tombol **RESET** (sambil tetap tahan BOOT)
3. Lepas **RESET**, tunggu "Connecting..." muncul di Arduino IDE
4. Lepas **BOOT**
5. Upload akan berjalan

---

## Prioritas Keputusan

```
PRIORITAS 1 (tertinggi): Rain Sensor fisik = HUJAN
  → TUTUP ATAP. Tidak bisa di-override oleh apapun.

PRIORITAS 2: Perintah manual dari user (via Supabase)
  → Eksekusi, KECUALI kalau hujan (Rule 1 menang).

PRIORITAS 3: Prediksi cuaca Open-Meteo >= 60% hujan
  → Tutup atap sebagai antisipasi.

PRIORITAS 4 (terendah): Logika sensor lokal
  → Cahaya > 500 lux + suhu < 33°C = buka
  → Cahaya < 500 lux = tutup (gelap/malam)
```

---

## Scheduling Task di ESP32

Semua task di-stagger supaya tidak ada 2 HTTPS request berjalan bersamaan (hemat RAM):

```
Tiap 5 detik   → Baca sensor (lokal, ringan)
Tiap 10 detik  → GET Supabase: cek perintah manual
Tiap 60 detik  → POST Supabase: kirim data sensor
Tiap 30 menit  → GET Open-Meteo: prediksi cuaca
Setiap loop    → Cek hysteresis servo
```

---

## Web Dashboard (Vercel + Next.js)

### Setup

```bash
npx create-next-app@latest greenhouse-dashboard
# TypeScript: Yes, Tailwind: Yes, App Router: Yes

cd greenhouse-dashboard
npm install @supabase/supabase-js
```

### Environment Variables

Buat file `.env.local`:
```
NEXT_PUBLIC_SUPABASE_URL=https://xxxxx.supabase.co
NEXT_PUBLIC_SUPABASE_ANON_KEY=eyJhbGciOiJ...
```

### Fitur Dashboard

- **Sensor Cards**: Suhu, Humidity, Cahaya, Status Hujan (real-time)
- **Roof Status**: Posisi atap + mode (Auto/Manual)
- **Control Panel**: Tombol Buka / Tutup / Auto
- **Grafik Historis**: Data 24 jam terakhir
- **Weather Forecast**: Prediksi cuaca dari Open-Meteo
- **Alert Banner**: Peringatan overheating

### Cara Kontrol Manual Bekerja

```
User klik "BUKA" di dashboard
  → Frontend INSERT ke Supabase: { action: "open", executed: false }
  → ESP32 GET /commands tiap 10 detik
  → ESP32 baca action "open", cek Rule 1
  → Kalau tidak hujan: servo target 180°, tunggu hysteresis 60s
  → ESP32 PATCH /commands: { executed: true }
  → ESP32 POST /sensor_logs: { roof_state: "open", mode: "manual" }
  → Dashboard auto-update via Supabase Realtime
```

### Real-time Update (Supabase Realtime)

```typescript
import { createClient } from '@supabase/supabase-js'

const supabase = createClient(
  process.env.NEXT_PUBLIC_SUPABASE_URL!,
  process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!
)

// Subscribe ke data sensor baru
const channel = supabase
  .channel('sensor-updates')
  .on(
    'postgres_changes',
    { event: 'INSERT', schema: 'public', table: 'sensor_logs' },
    (payload) => {
      // Update UI dengan data terbaru
      setSensorData(payload.new)
    }
  )
  .subscribe()
```

### Deploy

```bash
npm i -g vercel
vercel
# Set env vars di Vercel dashboard
```

---

## Testing

### Phase 1: Sensor + Servo Lokal

| Test | Cara | Expected |
|------|------|----------|
| Sensor baca normal | Power on, buka Serial Monitor | Data muncul tiap 5 detik |
| Rain → tutup | Teteskan air ke rain plate | Servo 0° setelah 60s |
| Cerah → buka | Lampu terang ke BH1750 | Servo 180° setelah 60s |
| Hysteresis | Air sebentar lalu keringkan < 60s | Servo TIDAK bergerak |
| Anti-oven | Air + panaskan DHT11 > 33°C | LED berkedip |
| Sensor error | Cabut kabel DHT11 | Error di Serial, servo diam |

### Phase 2: WiFi + Supabase

| Test | Cara | Expected |
|------|------|----------|
| WiFi connect | Power on dengan WiFi | Serial: "Terhubung! IP: ..." |
| Data ke Supabase | Tunggu 60 detik | Row baru di tabel sensor_logs |
| Manual open | Insert command di SQL Editor | Servo buka |
| Manual vs hujan | Kirim "open" saat hujan | Servo TETAP TUTUP |
| WiFi disconnect | Matikan router | Sistem tetap jalan lokal |

### Phase 3: Dashboard

| Test | Cara | Expected |
|------|------|----------|
| Dashboard load | Buka URL Vercel | Sensor cards tampil |
| Real-time | Tunggu data baru | Update tanpa refresh |
| Tombol BUKA | Klik di dashboard | Servo buka (kalau tidak hujan) |

---

## Troubleshooting

| Problem | Solusi |
|---------|--------|
| ESP32 tidak muncul di COM port | Ganti kabel USB (harus data, bukan charge-only) |
| Upload gagal "Connecting..." | Tahan BOOT, tekan RESET, lepas BOOT |
| DHT11 baca NaN | Cek kabel DATA di GPIO4, pastikan module 3-pin |
| BH1750 baca 0 | Cek SDA=GPIO8, SCL=GPIO9. Jalankan I2C Scanner |
| Rain sensor selalu LOW | Putar potensiometer di modul |
| Servo tidak bergerak | Cek power dari adaptor (bukan ESP32), cek GPIO13 |
| Servo getar/jitter | Pastikan capacitor 470µF terpasang |
| Supabase 401 | Cek API key di credentials.h (harus anon key) |
| Supabase 404 | Pastikan tabel sensor_logs dan commands sudah dibuat |
| ESP32 restart sendiri | Cek free heap di Serial Monitor. Jangan ada delay() |

---

## Power Budget

| Komponen | Normal | Peak |
|----------|--------|------|
| ESP32-S3 (WiFi aktif) | 150mA | 250mA |
| BH1750 + DHT11 + Rain + LED | 18mA | 48mA |
| Servo MG996R | 500mA | 2500mA (stall) |
| **Total** | **668mA** | **2798mA** |
| **Adaptor 5V 3A** | **22% load** | **93% load** |

---

## Lisensi

Proyek tugas kampus — ITB Semester 2, 2026.
