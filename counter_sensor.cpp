#include "globals.h"
#include "driver/gpio.h"  // gpio_get_level(): lectura segura desde ISR (IRAM)

// IMPORTANTE (hardware):
// El E3X-NA41 trabaja a 12-24V. La señal de su salida digital JAMÁS debe
// conectarse directamente a un GPIO del ESP32 (3.3V, no tolerante a 5V).
// Usar un optoacoplador (PC817) o módulo de interfaz opto-aislada entre
// la salida del amplificador y PIN_SENSOR.

static portMUX_TYPE debounceMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t lastEdgeMicros = 0;

// true  = sensor "armado", esperando a que llegue una prenda (reposo).
// false = ya se ha contado esta prenda, esperando a que salga del haz
//         para poder volver a armarse.
static volatile bool sensorArmed = true;

// Valor de debounce realmente usado por la ISR. Se inicializa con el
// valor de fábrica y se sobrescribe con appConfig.sensor_debounce_us
// en counterSensorInit().
volatile uint32_t sensorDebounceUs = SENSOR_DEBOUNCE_US;

void IRAM_ATTR onSensorPulseISR() {
  uint32_t now = micros();

  portENTER_CRITICAL_ISR(&debounceMux);

  // Filtro de rebote: ignora CUALQUIER flanco (de entrada o de salida)
  // que llegue antes de que pase sensorDebounceUs desde el último
  // flanco aceptado. Esto limpia el ruido tanto al entrar como al
  // salir del haz, sin depender de contar flancos sueltos.
  if ((uint32_t)(now - lastEdgeMicros) < sensorDebounceUs) {
    portEXIT_CRITICAL_ISR(&debounceMux);
    return;
  }

  int level = gpio_get_level((gpio_num_t)PIN_SENSOR);
  bool detected = SENSOR_ACTIVE_LOW ? (level == 0) : (level != 0);

  bool countThis = false;

  if (detected && sensorArmed) {
    // Transición reposo -> detección: AQUI es donde se cuenta.
    countThis = true;
    sensorArmed = false;
    lastEdgeMicros = now;
  } else if (!detected && !sensorArmed) {
    // Transición detección -> reposo: solo re-arma, NUNCA cuenta.
    sensorArmed = true;
    lastEdgeMicros = now;
  }
  // Si el flanco no representa un cambio de estado lógico válido
  // (p.ej. ruido que no coincide con el estado esperado), se ignora
  // sin tocar lastEdgeMicros ni sensorArmed.

  portEXIT_CRITICAL_ISR(&debounceMux);

  if (!countThis) return;

  sensorPulseCount++;  // contador crudo, solo diagnóstico

  portENTER_CRITICAL_ISR(&countMux);
  if (systemState == STATE_RUNNING && hasActiveArticle) {
    activeCount++;
  }
  portEXIT_CRITICAL_ISR(&countMux);
}

void counterSensorInit() {
  sensorDebounceUs = appConfig.sensor_debounce_us;
  sensorArmed = true;

  pinMode(PIN_SENSOR, SENSOR_ACTIVE_LOW ? INPUT_PULLUP : INPUT);

  // CHANGE en lugar de solo FALLING/RISING: necesitamos ver ambos
  // flancos para poder re-armar el sensor cuando la prenda sale del
  // haz. El propio mecanismo de armado/desarmado de arriba es lo que
  // garantiza que solo se cuenta al entrar, nunca al salir.
  attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), onSensorPulseISR, CHANGE);
}
