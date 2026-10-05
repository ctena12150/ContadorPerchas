#include "globals.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// ============================================================
// Configuración
// ============================================================

// Lectura cada 250 microsegundos.
static constexpr uint32_t SENSOR_SAMPLE_US = 250;

// Número de lecturas iguales necesarias para confirmar un cambio.
// 4 lecturas x 250 us = aproximadamente 1 ms.
static constexpr uint8_t SENSOR_STABLE_SAMPLES = 4;

// Protección adicional entre conteos.
// Evita dobles conteos provocados por ruido fuerte.
//
// 3000 us permite teóricamente más de 300 perchas por segundo.
// Si las perchas están extremadamente juntas, puedes bajarlo a 1500.
static constexpr uint32_t SENSOR_MIN_COUNT_INTERVAL_US = 3000;

// ============================================================
// Estado interno
// ============================================================

static esp_timer_handle_t sensorTimer = nullptr;

static portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;

// Estado eléctrico sin procesar.
static volatile bool sensorRawOn = false;

// Último estado estable confirmado.
static volatile bool sensorStableOn = false;

// Número de lecturas consecutivas iguales.
static volatile uint8_t sensorStableSamples = 0;

// Evita contar varias veces mientras el sensor permanece apagado.
//
// true  = se permite contar el próximo ON -> OFF.
// false = ya se contó; debe volver a ON antes de rearmarse.
static volatile bool sensorArmed = false;

static volatile uint32_t lastCountMicros = 0;

// Se conserva por compatibilidad con la configuración existente,
// aunque este sistema usa SENSOR_STABLE_SAMPLES.
volatile uint32_t sensorDebounceUs = SENSOR_DEBOUNCE_US;

// ============================================================
// Lectura del sensor
// ============================================================

// Devuelve true cuando la salida del sensor está ENCENDIDA.
// Devuelve false cuando la salida del sensor está APAGADA.
static inline bool readSensorOn()
{
    int level = gpio_get_level((gpio_num_t)PIN_SENSOR);

    return SENSOR_ACTIVE_LOW
        ? (level == 0)
        : (level != 0);
}

// ============================================================
// Incremento del contador
// ============================================================

static void registerHanger()
{
    sensorPulseCount++;

    portENTER_CRITICAL(&countMux);

    if (systemState == STATE_RUNNING && hasActiveArticle) {
        activeCount++;
    }

    portEXIT_CRITICAL(&countMux);
}

// ============================================================
// Muestreo periódico
// ============================================================

static void sensorTimerCallback(void *arg)
{
    bool rawOn = readSensorOn();
    bool countThis = false;
    uint32_t now = micros();

    portENTER_CRITICAL(&sensorMux);

    // La lectura ha cambiado: empezamos de nuevo la validación.
    if (rawOn != sensorRawOn) {
        sensorRawOn = rawOn;
        sensorStableSamples = 1;

        portEXIT_CRITICAL(&sensorMux);
        return;
    }

    // La lectura sigue igual.
    if (sensorStableSamples < SENSOR_STABLE_SAMPLES) {
        sensorStableSamples++;
    }

    // Todavía no se ha mantenido estable el tiempo necesario.
    if (sensorStableSamples < SENSOR_STABLE_SAMPLES) {
        portEXIT_CRITICAL(&sensorMux);
        return;
    }

    // Ya coincide con el último estado estable confirmado.
    if (rawOn == sensorStableOn) {
        portEXIT_CRITICAL(&sensorMux);
        return;
    }

    // Confirmamos el nuevo estado estable.
    bool previousStableOn = sensorStableOn;
    sensorStableOn = rawOn;

    if (rawOn) {
        // El sensor ha vuelto a ENCENDIDO.
        //
        // Esto rearma el contador para que el siguiente
        // ENCENDIDO -> APAGADO pueda contar otra percha.
        sensorArmed = true;
    }
    else if (previousStableOn && sensorArmed) {
        // Cambio estable:
        //
        //     ENCENDIDO -> APAGADO
        //
        // Este es el único punto donde se permite contar.

        uint32_t elapsed = now - lastCountMicros;

        if (lastCountMicros == 0 ||
            elapsed >= SENSOR_MIN_COUNT_INTERVAL_US) {

            countThis = true;
            lastCountMicros = now;
        }

        // Se desarma siempre, incluso si el cambio fue demasiado rápido.
        // Mientras permanezca apagado no podrá volver a contar.
        sensorArmed = false;
    }

    portEXIT_CRITICAL(&sensorMux);

    if (countThis) {
        registerHanger();
    }
}

// ============================================================
// Inicialización
// ============================================================

void counterSensorInit()
{
    sensorDebounceUs = appConfig.sensor_debounce_us;

    // Si la salida utiliza un PC817 con colector abierto:
    // normalmente debe utilizarse INPUT_PULLUP y SENSOR_ACTIVE_LOW=true.
    pinMode(
        PIN_SENSOR,
        SENSOR_ACTIVE_LOW ? INPUT_PULLUP : INPUT
    );

    delayMicroseconds(100);

    bool initialOn = readSensorOn();

    portENTER_CRITICAL(&sensorMux);

    sensorRawOn = initialOn;
    sensorStableOn = initialOn;
    sensorStableSamples = SENSOR_STABLE_SAMPLES;

    // Si arranca encendido, queda preparado para contar cuando se apague.
    // Si arranca apagado, no cuenta hasta que vuelva a encenderse.
    sensorArmed = initialOn;

    lastCountMicros = 0;

    portEXIT_CRITICAL(&sensorMux);

    if (sensorTimer == nullptr) {
        esp_timer_create_args_t timerArgs = {};

        timerArgs.callback = &sensorTimerCallback;
        timerArgs.arg = nullptr;
        timerArgs.dispatch_method = ESP_TIMER_TASK;
        timerArgs.name = "hanger_sensor";
        timerArgs.skip_unhandled_events = true;

        esp_err_t result = esp_timer_create(
            &timerArgs,
            &sensorTimer
        );

        if (result != ESP_OK) {
            Serial.printf(
                "[SENSOR] Error creando temporizador: %d\n",
                result
            );

            sensorTimer = nullptr;
            return;
        }
    }

    esp_timer_stop(sensorTimer);

    esp_err_t result = esp_timer_start_periodic(
        sensorTimer,
        SENSOR_SAMPLE_US
    );

    if (result == ESP_OK) {
        Serial.printf(
            "[SENSOR] Iniciado. Estado=%s, muestra=%lu us, filtro=%lu us\n",
            initialOn ? "ON" : "OFF",
            (unsigned long)SENSOR_SAMPLE_US,
            (unsigned long)(
                SENSOR_SAMPLE_US * SENSOR_STABLE_SAMPLES
            )
        );
    }
    else {
        Serial.printf(
            "[SENSOR] Error iniciando temporizador: %d\n",
            result
        );
    }
}

// ============================================================
// Parada opcional
// ============================================================

void counterSensorStop()
{
    if (sensorTimer != nullptr &&
        esp_timer_is_active(sensorTimer)) {

        esp_timer_stop(sensorTimer);
    }
}
