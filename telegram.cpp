#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "globals.h"

static uint32_t lastNotifyMillis = 0;

void telegramInit() {
    lastNotifyMillis = 0;
}

void telegramNotifyError(const String &msg) {
    if (strlen(appConfig.tg_bot_token) == 0 || strlen(appConfig.tg_chat_id) == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    uint32_t now = millis();
    // Evitar saturar Telegram si el envío falla repetidamente
    if (now - lastNotifyMillis < TELEGRAM_MIN_INTERVAL_MS) return;
    lastNotifyMillis = now;

    WiFiClientSecure client;
    client.setInsecure(); // simplifica el ejemplo; en producción validar el certificado

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(appConfig.tg_bot_token) + "/sendMessage";

    if (!http.begin(client, url)) return;
    http.addHeader("Content-Type", "application/json");

    String text = "Contador de perchas - ERROR:\n" + msg;
    String body = "{\"chat_id\":\"" + String(appConfig.tg_chat_id) +
                   "\",\"text\":\"" + text + "\"}";

    http.POST(body);
    http.end();
}
