#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "globals.h"

static char isoBuf[32];

static const char *toIso8601(time_t t) {
    struct tm tmStruct;
    gmtime_r(&t, &tmStruct);
    strftime(isoBuf, sizeof(isoBuf), "%Y-%m-%dT%H:%M:%SZ", &tmStruct);
    return isoBuf;
}

bool postEnvioToEndpoint(const EnvioRecord &env) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (strlen(appConfig.api_endpoint) == 0) return false;

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(appConfig.api_endpoint)) return false;
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["envio_id"] = env.id;
    doc["dispositivo"] = appConfig.device_id;
    doc["centro"] = appConfig.centro_id;    
    doc["creado"] = toIso8601(env.createdAt);

    JsonArray arr = doc["articulos"].to<JsonArray>();
    for (const auto &a : env.articles) {
        JsonObject o = arr.add<JsonObject>();
        o["barcode"] = a.barcode;
        o["start_time"] = toIso8601(a.startTime);
        o["count"] = a.count;
    }

    String body;
    serializeJson(doc, body);
    Serial.println("Envio"); 
    Serial.println(body); 
    int code = http.POST(body);
    http.end();

    // Se considera éxito cualquier 2xx
    return (code >= 200 && code < 300);
}

static void sendTaskFn(void *param) {
    for (;;) {
        EnvioRecord env;
        if (peekPendingEnvio(env)) {
            bool ok = postEnvioToEndpoint(env);
            if (ok) {
                popPendingEnvio();
                uiClearError();
            } else {
                String msg = "Error enviando " + String(env.id) + " (" +
                             String((unsigned)env.articles.size()) + " articulos, " +
                             String((unsigned)pendingEnvioCount()) + " envios en cola)";
                uiSetError(msg);
                telegramNotifyError(msg);
                vTaskDelay(pdMS_TO_TICKS(HTTP_SEND_RETRY_DELAY_MS));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void sendTaskStart() {
    xTaskCreatePinnedToCore(
        sendTaskFn,
        "send_task",
        8192,   // stack algo mayor: JSON con varios artículos por envío
        nullptr,
        1,      // prioridad más baja que el conteo
        nullptr,
        0       // core 0, separado de la tarea de conteo/UI (core 1)
    );
}
