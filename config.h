#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>

// ============================================================
//  PINES — ESP32-S3
// ============================================================
// La pantalla/táctil usan el pinout que YA tienes probado y
// funcionando con TFT_eSPI (ver User_Setup.h en el README):
//   TFT_MOSI 11, TFT_SCLK 12, TFT_CS 10, TFT_DC 9, TFT_RST 8, TOUCH_CS 15
// Esos pines NO se usan aquí: TFT_eSPI los toma directamente de su
// propio User_Setup.h, no de este fichero.

// --- Lector de códigos QR/barras (UART) ---
#define PIN_QR_RX      17   // <- TX del lector
#define PIN_QR_TX      16   // -> RX del lector (no siempre necesario)
#define QR_BAUDRATE    9600 // habitual en lectores serie, revisar manual del lector

// --- Sensor de fibra óptica E3X-NA41 (salida digital) ---
// IMPORTANTE: el amplificador E3X-NA41 se alimenta a 12-24V y su salida
// NPN/PNP puede trabajar a ese nivel. El ESP32-S3 NO es tolerante a
// 5V/12V/24V. Es OBLIGATORIO interponer un optoacoplador (p.ej. PC817) o
// un módulo "level shifter"/interfaz opto-aislada antes de conectar la
// señal a este GPIO. Nunca conectar la salida del sensor directamente.
// GPIO 5: mismo pin que "PIN_PRENDA" en tu código de referencia.
#define PIN_SENSOR     5
#define SENSOR_ACTIVE_LOW   true   // salida NPN típica -> pulso a GND = detección
#define SENSOR_DEBOUNCE_US  100   // ajustar según velocidad real de las perchas

// ============================================================
//  PARÁMETROS DE APLICACIÓN
// ============================================================
#define BARCODE_MAX_LEN          64
#define ENVIO_ID_LEN              40
#define MAX_PENDING_ENVIOS        15   // envíos en cola (colchón si se cae la red)
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define AP_SSID                  "ESP32-Perchas-Config"
#define AP_PASSWORD               "config1234"
#define WEBSERVER_PORT            80
#define TELEGRAM_MIN_INTERVAL_MS  60000  // no saturar Telegram con reintentos
#define HTTP_SEND_RETRY_DELAY_MS  5000

// Estado del sistema
enum SystemState : uint8_t {
    STATE_IDLE = 0,
    STATE_RUNNING = 1
};

// Un artículo leído dentro de una sesión: código + hora de inicio + conteo
struct ArticleRecord {
    char barcode[BARCODE_MAX_LEN];
    time_t startTime;
    uint32_t count;
};

// Un envío es el conjunto de artículos leídos entre dos pulsaciones de
// "Inicio"/"Fin". Lleva un identificador único para poder diferenciarlo
// de otros envíos en la base de datos.
struct EnvioRecord {
    char id[ENVIO_ID_LEN];
    time_t createdAt;
    std::vector<ArticleRecord> articles;
};

// Configuración persistente (se guarda en NVS/Preferences)
struct AppConfig {
    char wifi_ssid[33]  = "";
    char wifi_pass[65]  = "";
    char ntp_server[65] = "pool.ntp.org";
    long  gmt_offset_sec = 3600;      // España peninsular UTC+1
    int   dst_offset_sec = 3600;      // horario de verano
    char api_endpoint[128] = "https://tuservidor.example.com/api/perchas";
    char tg_bot_token[64]  = "";
    char tg_chat_id[32]    = "";
    // Debounce del sensor de fibra óptica (microsegundos).
  // Editable desde el portal web sin recompilar ni resubir firmware.
  uint32_t sensor_debounce_us = SENSOR_DEBOUNCE_US;

  // Credenciales de acceso al portal web de configuración.
  // Si AMBOS campos están vacíos (estado de fábrica / primer arranque)
  // el portal no pide usuario/contraseña. En cuanto se rellena alguno
  // desde la propia web, todo el portal queda protegido con HTTP Basic Auth.
  char web_user[33] = "";
  char web_pass[65] = "";
    // Identificador de este dispositivo. Se envia en cada POST para que
  // el backend sepa de que maquina/puesto viene cada envio. Editable
  // desde el portal web; util cuando hay varios ESP32 enviando al
  // mismo endpoint (p.ej. "Linea1-PuestoA").
  char device_id[33] = "";
    // Centro / tienda / almacen al que pertenece este dispositivo.
  // Se envia junto al dispositivo en cada POST para poder agrupar
  // y filtrar en el backend por centro. Editable desde la web.
  char centro_id[33] = "";

};
