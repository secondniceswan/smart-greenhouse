#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// Inisialisasi WiFi (panggil sekali di setup())
// Akan mencoba connect selama max 10 detik
// Kalau gagal, lanjut mode offline (tidak crash)
void wifiInit();

// Cek apakah WiFi sedang terhubung
bool wifiIsConnected();

// Panggil di setiap loop()
// Kalau WiFi putus, akan coba reconnect secara non-blocking tiap 30 detik
// TIDAK akan block loop utama — sensor dan servo tetap jalan
void wifiReconnectIfNeeded();

#endif
