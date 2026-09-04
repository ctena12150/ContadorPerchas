#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include "config.h"

extern TFT_eSPI tft;
extern AppConfig appConfig;

// Estado de sesión (IDLE / RUNNING)
extern volatile SystemState systemState;
extern portMUX_TYPE stateMux;

// Sesión actual: lista de artículos leídos desde el último "Inicio".
// El último elemento del vector es siempre el artículo activo (el que
// está recibiendo los pulsos del sensor en este momento).
struct CurrentSession {
    std::vector<ArticleRecord> articles;
};
extern CurrentSession currentSession;
extern SemaphoreHandle_t sessionMutex;

// Conteo "en vivo" del artículo activo. Se actualiza desde la ISR del
// sensor, por eso va protegido con un spinlock (portMUX), no con un
// mutex normal (los mutex de FreeRTOS no se pueden tomar desde una ISR).
extern volatile uint32_t activeCount;
extern volatile bool hasActiveArticle;
extern portMUX_TYPE countMux;

// Contador crudo de pulsos del sensor (diagnóstico)
extern volatile uint32_t sensorPulseCount;



// Debounce del sensor en microsegundos, leído desde la ISR.
// Se sincroniza con appConfig.sensor_debounce_us en counterSensorInit()
// y cada vez que se guarda configuración nueva desde el portal web.
extern volatile uint32_t sensorDebounceUs;

// Cola de envíos pendientes de mandar al servidor
extern SemaphoreHandle_t pendingMutex;

// Bandera de error visible en pantalla (protegida por errorMutex)
extern SemaphoreHandle_t errorMutex;
extern char lastErrorMsg[128];
extern bool hasError;

// ---- gestión de datos (data_manager) ----
void dataManagerInit();
void startSession();
void endSession();
void handleNewBarcode(const char *code);

// Accesores de solo lectura para la UI (protegidos internamente por mutex)
size_t sessionArticleCount();
bool getSessionArticle(size_t index, ArticleRecord &out); // 0 = primero leído ... count-1 = activo
uint32_t getActiveLiveCount();
uint32_t sessionTotalCount();

void enqueuePendingEnvio(const EnvioRecord &env);
bool peekPendingEnvio(EnvioRecord &out);
void popPendingEnvio();
size_t pendingEnvioCount();

// ---- sensor ----
void counterSensorInit();

// ---- lector QR ----
void qrReaderTaskStart();

// ---- envío HTTP ----
void sendTaskStart();
bool postEnvioToEndpoint(const EnvioRecord &env);

// ---- telegram ----
void telegramInit();
void telegramNotifyError(const String &msg);

// ---- red / NTP ----
bool connectWiFiSTA();
void startConfigAP();
void syncTimeNTP();

// ---- webserver de configuración ----
void configWebServerStart();
void configWebServerHandle();

// ---- almacenamiento NVS ----
void storageLoad(AppConfig &cfg);
void storageSave(const AppConfig &cfg);
void storageLoadTouchCalibration(uint16_t *calData, bool &valid);
void storageSaveTouchCalibration(const uint16_t *calData);
void storageClearTouchCalibration();

// ---- UI ----
void uiInit();
void uiLoop();
void uiSetError(const String &msg);
void uiClearError();
void uiRunCalibration();
