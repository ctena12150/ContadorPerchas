#include <Preferences.h>
#include "globals.h"

static Preferences prefs;

void storageLoad(AppConfig &cfg) {
    prefs.begin("appcfg", true); // solo lectura
    String s;

    s = prefs.getString("wifi_ssid", cfg.wifi_ssid);
    s.toCharArray(cfg.wifi_ssid, sizeof(cfg.wifi_ssid));

    s = prefs.getString("wifi_pass", cfg.wifi_pass);
    s.toCharArray(cfg.wifi_pass, sizeof(cfg.wifi_pass));

    s = prefs.getString("ntp_server", cfg.ntp_server);
    s.toCharArray(cfg.ntp_server, sizeof(cfg.ntp_server));

    cfg.gmt_offset_sec = prefs.getLong("gmt_off", cfg.gmt_offset_sec);
    cfg.dst_offset_sec = prefs.getInt("dst_off", cfg.dst_offset_sec);

    s = prefs.getString("api_endpoint", cfg.api_endpoint);
    s.toCharArray(cfg.api_endpoint, sizeof(cfg.api_endpoint));

    s = prefs.getString("tg_token", cfg.tg_bot_token);
    s.toCharArray(cfg.tg_bot_token, sizeof(cfg.tg_bot_token));

    s = prefs.getString("tg_chat", cfg.tg_chat_id);
    s.toCharArray(cfg.tg_chat_id, sizeof(cfg.tg_chat_id));

    prefs.end();
}

void storageSave(const AppConfig &cfg) {
    prefs.begin("appcfg", false);
    prefs.putString("wifi_ssid", cfg.wifi_ssid);
    prefs.putString("wifi_pass", cfg.wifi_pass);
    prefs.putString("ntp_server", cfg.ntp_server);
    prefs.putLong("gmt_off", cfg.gmt_offset_sec);
    prefs.putInt("dst_off", cfg.dst_offset_sec);
    prefs.putString("api_endpoint", cfg.api_endpoint);
    prefs.putString("tg_token", cfg.tg_bot_token);
    prefs.putString("tg_chat", cfg.tg_chat_id);
    prefs.end();
}

// TFT_eSPI::calibrateTouch() genera 5 uint16_t (no 8 como LovyanGFX)
#define TOUCH_CAL_WORDS 5

void storageLoadTouchCalibration(uint16_t *calData, bool &valid) {
    prefs.begin("touchcal", true);
    size_t len = prefs.getBytes("caldata", calData, TOUCH_CAL_WORDS * sizeof(uint16_t));
    valid = (len == TOUCH_CAL_WORDS * sizeof(uint16_t));
    prefs.end();
}

void storageSaveTouchCalibration(const uint16_t *calData) {
    prefs.begin("touchcal", false);
    prefs.putBytes("caldata", calData, TOUCH_CAL_WORDS * sizeof(uint16_t));
    prefs.end();
    Serial.printf("[CAL] Guardado: %u %u %u %u %u\n",
                  calData[0], calData[1], calData[2], calData[3], calData[4]);
}

void storageClearTouchCalibration() {
    prefs.begin("touchcal", false);
    prefs.clear();
    prefs.end();
    Serial.println("[CAL] Calibracion borrada de NVS");
}
