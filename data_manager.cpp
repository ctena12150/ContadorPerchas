#include <vector>
#include <esp_random.h>
#include "globals.h"

// ---- Definición de las variables globales declaradas en globals.h ----
volatile SystemState systemState = STATE_IDLE;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE countMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t activeCount = 0;
volatile bool hasActiveArticle = false;
volatile uint32_t sensorPulseCount = 0;

CurrentSession currentSession;
SemaphoreHandle_t sessionMutex = nullptr;
SemaphoreHandle_t pendingMutex = nullptr;
SemaphoreHandle_t errorMutex   = nullptr;
char lastErrorMsg[128] = "";
bool hasError = false;

static std::vector<EnvioRecord> pendingEnvios;

void dataManagerInit() {
    sessionMutex = xSemaphoreCreateMutex();
    pendingMutex = xSemaphoreCreateMutex();
    errorMutex   = xSemaphoreCreateMutex();
}

// Genera un identificador único para el envío: fecha/hora + sufijo aleatorio.
// Ej: EV-20260818T101530-A1B2
static String generateEnvioId() {
    time_t now = time(nullptr);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    char ts[20];
    strftime(ts, sizeof(ts), "%Y%m%dT%H%M%S", &tmv);

    uint32_t r = esp_random() & 0xFFFF;
    char id[ENVIO_ID_LEN];
    snprintf(id, sizeof(id), "EV-%s-%04X", ts, (unsigned)r);
    return String(id);
}

// Se pulsa "Inicio" estando en IDLE -> arranca una nueva sesión de lectura
void startSession() {
    if (xSemaphoreTake(sessionMutex, portMAX_DELAY) == pdTRUE) {
        currentSession.articles.clear();
        xSemaphoreGive(sessionMutex);
    }

    portENTER_CRITICAL(&countMux);
    activeCount = 0;
    hasActiveArticle = false;
    portEXIT_CRITICAL(&countMux);

    portENTER_CRITICAL(&stateMux);
    systemState = STATE_RUNNING;
    portEXIT_CRITICAL(&stateMux);
}

// Se pulsa "Inicio"/"Fin" estando en RUNNING -> cierra el artículo activo
// (si lo hay), empaqueta TODOS los artículos de la sesión en un único
// envío con ID propio, lo encola para su envío y vuelve a IDLE.
void endSession() {
    if (xSemaphoreTake(sessionMutex, portMAX_DELAY) != pdTRUE) return;

    bool hasArticles = !currentSession.articles.empty();
    EnvioRecord env;

    if (hasArticles) {
        // Cerrar el conteo del artículo activo con su valor definitivo
        uint32_t finalCount;
        portENTER_CRITICAL(&countMux);
        finalCount = activeCount;
        activeCount = 0;
        hasActiveArticle = false;
        portEXIT_CRITICAL(&countMux);
        currentSession.articles.back().count = finalCount;

        String id = generateEnvioId();
        strncpy(env.id, id.c_str(), sizeof(env.id) - 1);
        env.id[sizeof(env.id) - 1] = '\0';
        env.createdAt = time(nullptr);
        env.articles = currentSession.articles; // copia: n artículos de esta sesión
    }

    currentSession.articles.clear();
    xSemaphoreGive(sessionMutex);

    portENTER_CRITICAL(&stateMux);
    systemState = STATE_IDLE;
    portEXIT_CRITICAL(&stateMux);

    if (hasArticles) {
        enqueuePendingEnvio(env);
    }
}

// Llegada de un nuevo código de barras/QR.
// Si el sistema está parado (IDLE), la propia lectura arranca la sesión
// automáticamente (equivale a pulsar "Inicio"), y a continuación se
// registra el código igual que si ya estuviera en marcha.
void handleNewBarcode(const char *code) {
    if (systemState != STATE_RUNNING) {
        startSession();
    }
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(300)) != pdTRUE) return;

    if (!currentSession.articles.empty()) {
        uint32_t finalCount;
        portENTER_CRITICAL(&countMux);
        finalCount = activeCount;
        portEXIT_CRITICAL(&countMux);
        currentSession.articles.back().count = finalCount;
    }

    ArticleRecord rec;
    strncpy(rec.barcode, code, BARCODE_MAX_LEN - 1);
    rec.barcode[BARCODE_MAX_LEN - 1] = '\0';
    rec.startTime = time(nullptr);
    rec.count = 0;
    currentSession.articles.push_back(rec);

    // El nuevo artículo pasa a ser el activo: el contador en vivo se reinicia.
    // Esto también descarta cualquier pulso que hubiera llegado antes de
    // leer el primer código (no debe contar para ningún artículo).
    portENTER_CRITICAL(&countMux);
    activeCount = 0;
    hasActiveArticle = true;
    portEXIT_CRITICAL(&countMux);

    xSemaphoreGive(sessionMutex);
}

// ---- Accesores de solo lectura para la UI ----

size_t sessionArticleCount() {
    size_t n = 0;
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        n = currentSession.articles.size();
        xSemaphoreGive(sessionMutex);
    }
    return n;
}

bool getSessionArticle(size_t index, ArticleRecord &out) {
    bool found = false;
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (index < currentSession.articles.size()) {
            out = currentSession.articles[index];
            found = true;
        }
        xSemaphoreGive(sessionMutex);
    }
    return found;
}

uint32_t getActiveLiveCount() {
    uint32_t c;
    portENTER_CRITICAL(&countMux);
    c = activeCount;
    portEXIT_CRITICAL(&countMux);
    return c;
}

// ---- Cola de envíos pendientes de mandar al servidor ----

void enqueuePendingEnvio(const EnvioRecord &env) {
    if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
        if (pendingEnvios.size() >= MAX_PENDING_ENVIOS) {
            // Colchón lleno (red caída mucho tiempo): se descarta el más
            // antiguo para no perder el envío más reciente.
            pendingEnvios.erase(pendingEnvios.begin());
        }
        pendingEnvios.push_back(env);
        xSemaphoreGive(pendingMutex);
    }
}

bool peekPendingEnvio(EnvioRecord &out) {
    bool found = false;
    if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
        if (!pendingEnvios.empty()) {
            out = pendingEnvios.front();
            found = true;
        }
        xSemaphoreGive(pendingMutex);
    }
    return found;
}

void popPendingEnvio() {
    if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
        if (!pendingEnvios.empty()) {
            pendingEnvios.erase(pendingEnvios.begin());
        }
        xSemaphoreGive(pendingMutex);
    }
}

size_t pendingEnvioCount() {
    size_t n = 0;
    if (xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
        n = pendingEnvios.size();
        xSemaphoreGive(pendingMutex);
    }
    return n;
}
