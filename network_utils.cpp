#include <WiFi.h>
#include "globals.h"

bool connectWiFiSTA() {
    if (strlen(appConfig.wifi_ssid) == 0) return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(appConfig.wifi_ssid, appConfig.wifi_pass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

void startConfigAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
}

void syncTimeNTP() {
    if (WiFi.status() != WL_CONNECTED) return;
    configTime(appConfig.gmt_offset_sec, appConfig.dst_offset_sec, appConfig.ntp_server);

    // Espera breve a que se sincronice (no bloquear indefinidamente)
    struct tm timeinfo;
    uint32_t start = millis();
    while (!getLocalTime(&timeinfo, 500) && (millis() - start) < 8000) {
        // reintentando
    }
}
