#include "globals.h"

// IMPORTANTE (hardware):
// El E3X-NA41 trabaja a 12-24V. La señal de su salida digital JAMÁS debe
// conectarse directamente a un GPIO del ESP32 (3.3V, no tolerante a 5V).
// Usar un optoacoplador (PC817) o módulo de interfaz opto-aislada entre
// la salida del amplificador y PIN_SENSOR.

static portMUX_TYPE debounceMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t lastPulseMicros = 0;

void IRAM_ATTR onSensorPulseISR() {
    uint32_t now = micros();

    // Debounce por tiempo mínimo entre flancos válidos (independiente de
    // si hay o no artículo activo, para no perder la referencia temporal).
    bool accept = false;
    portENTER_CRITICAL_ISR(&debounceMux);
    if ((uint32_t)(now - lastPulseMicros) >= SENSOR_DEBOUNCE_US) {
        lastPulseMicros = now;
        accept = true;
    }
    portEXIT_CRITICAL_ISR(&debounceMux);

    if (!accept) return;

    sensorPulseCount++; // contador crudo, solo diagnóstico

    // Solo suma al artículo activo si el sistema está en marcha y ya se
    // ha leído al menos un código de barras en esta sesión.
    portENTER_CRITICAL_ISR(&countMux);
    if (systemState == STATE_RUNNING && hasActiveArticle) {
        activeCount++;
    }
    portEXIT_CRITICAL_ISR(&countMux);
}

void counterSensorInit() {
    pinMode(PIN_SENSOR, SENSOR_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR),
                     onSensorPulseISR,
                     SENSOR_ACTIVE_LOW ? FALLING : RISING);
}
