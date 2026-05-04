#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// Smart Greenhouse IoT — Konfigurasi Pin & Konstanta
// Board: ESP32-S3-N16R8 Soldered
// =============================================================================

// ======================== PIN DEFINITION (ESP32-S3) ========================
// PENTING: ESP32-S3 punya default I2C yang BERBEDA dari ESP32 biasa.
//   ESP32 biasa: SDA=GPIO21, SCL=GPIO22
//   ESP32-S3:    SDA=GPIO8,  SCL=GPIO9
#define PIN_SDA           8      // GPIO8  → BH1750 SDA (default I2C SDA pada S3)
#define PIN_SCL           9      // GPIO9  → BH1750 SCL (default I2C SCL pada S3)
#define PIN_DHT11         4      // GPIO4  → DHT11 DATA
#define PIN_RAIN_DO       5      // GPIO5  → Rain Sensor Digital Output (DO)
#define PIN_LED_RED       7      // GPIO7  → LED Merah: nyala = atap TUTUP, mati = atap BUKA
#define PIN_FAN           12     // GPIO12 → LED Hijau (simulasi KIPAS): nyala = kipas ON
#define PIN_LED_WARNING   PIN_LED_RED   // Alias kompatibilitas
#define PIN_SERVO         13     // GPIO13 → Servo MG996R (opsional, belum dipasang)

// ======================== INDIKATOR ATAP (pakai LED merah, bukan servo) ========================
// Belum pakai servo MG996R — LED merah jadi indikator state atap.
// LED merah ON  = atap "TUTUP"
// LED merah OFF = atap "BUKA"
// Kipas (LED hijau) dikontrol terpisah, lihat decision.cpp.
#define SERVO_OPEN        180    // Nilai simbolik untuk state BUKA (kompatibel API lama)
#define SERVO_CLOSED      0      // Nilai simbolik untuk state TUTUP

// ======================== SENSOR THRESHOLDS ========================
// Nilai-nilai ini bisa di-tuning sesuai kondisi greenhouse Anda
#define TEMP_OVERHEAT     33.0   // °C — di atas ini = warning anti-oven
#define LUX_LOW           500    // lux — di bawah ini = gelap/mendung, tutup atap
#define LUX_HIGH          10000  // lux — di atas ini = terik matahari
#define HUMIDITY_HIGH     85.0   // % — kelembapan tinggi (untuk referensi)

// ======================== TIMING (dalam milidetik) ========================
// Semua timing pakai millis(), JANGAN PERNAH pakai delay() di loop utama
#define SENSOR_READ_INTERVAL     5000      // Baca sensor tiap 5 detik
#define HYSTERESIS_DELAY         5000      // Tunggu 5 detik kondisi stabil sebelum gerak (testing; produksi: 60000)
#define LED_BLINK_INTERVAL       500       // LED warning kedip tiap 500ms
#define WIFI_RECONNECT_INTERVAL  30000     // Coba reconnect WiFi tiap 30 detik
#define SUPABASE_POST_INTERVAL   60000     // Kirim data sensor ke Supabase tiap 60 detik
#define SUPABASE_CHECK_INTERVAL  10000     // Cek perintah manual dari Supabase tiap 10 detik
#define WEATHER_FETCH_INTERVAL   1800000   // Ambil forecast Open-Meteo tiap 30 menit

// ======================== RAIN SENSOR LOGIC ========================
// Modul rain sensor MH-RD + LM393:
//   DO = LOW  saat ada air (hujan)
//   DO = HIGH saat kering (tidak hujan)
// PENTING: Ini bisa berbeda tergantung modul. Cek dulu pakai Serial Monitor.
//   Kalau terbalik, ganti RAIN_DETECTED ke HIGH.
#define RAIN_DETECTED     LOW

// TESTING: kalau true, sensor hujan tetap dibaca & dikirim,
// TAPI tidak akan paksa atap tutup / tolak manual override.
// Set false di produksi.
#define IGNORE_RAIN_OVERRIDE  true

// ======================== WEATHER API ========================
// Koordinat: Jatinangor, Sumedang
#define LATITUDE          "-6.9272"
#define LONGITUDE         "107.7755"
#define RAIN_PROBABILITY_THRESHOLD  60

// NTP — WIB = UTC+7
#define NTP_SERVER        "pool.ntp.org"
#define GMT_OFFSET_SEC    25200
#define DAYLIGHT_OFFSET   0

// ======================== HTTP ========================
#define HTTP_TIMEOUT      5000   // Timeout untuk request ke Supabase (ms)
#define WEATHER_TIMEOUT   10000  // Timeout untuk request ke Open-Meteo (ms)

#endif
