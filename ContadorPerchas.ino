// ============================================================
//  Contador de perchas - ESP32-S3 + ILI9488 (3.5") + XPT2046
// ============================================================
//  Requiere, instaladas desde el Gestor de Librerias de Arduino IDE:
//    - TFT_eSPI            (por Bodmer)
//    - ArduinoJson         (por bblanchon)
//  Junto con el core "esp32" de Espressif instalado desde el
//  Gestor de Placas (Board Manager).
//
//  IMPORTANTE: antes de compilar hay que configurar
//  User_Setup.h de la libreria TFT_eSPI. Ver README.md.
//
//  Placa: "ESP32S3 Dev Module"
// ============================================================

#include "globals.h"

TFT_eSPI tft = TFT_eSPI();
AppConfig appConfig;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Iniciando contador de perchas...");

    // --- Estructuras de datos y mutex ---
    dataManagerInit();

    // --- Pantalla ---
    tft.init();
    tft.setRotation(1); // horizontal 480x320 - ajustar 0-3 segun orientacion fisica
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Cargando configuracion...");

    // --- Configuración persistente ---
    storageLoad(appConfig);

    // --- Sensor de conteo (fibra optica E3X-NA41) ---
    counterSensorInit();

    // --- WiFi: intenta STA, si falla monta un AP de configuracion ---
    tft.println("Conectando WiFi...");
    bool connected = connectWiFiSTA();
    if (!connected) {
        tft.println("Sin WiFi: modo AP de configuracion");
        tft.println("SSID: " AP_SSID);
        tft.println("IP:   192.168.4.1");
        startConfigAP();
    } else {
        tft.print("WiFi OK: ");
        tft.println(WiFi.localIP());
        syncTimeNTP();
    }

    // --- Portal web de configuracion (disponible en STA o en AP) ---
    configWebServerStart();

    // --- Telegram ---
    telegramInit();

    // --- Interfaz tactil (calibra si no hay datos guardados) ---
    uiInit();

    // --- Tareas independientes ---
    qrReaderTaskStart();  // lectura del lector de codigos por UART
    sendTaskStart();      // envio al endpoint HTTP, en tarea/nucleo separado

    Serial.println("Sistema listo.");
}

void loop() {
    // Comando de depuración por Monitor Serie: escribe "CAL" + Enter para
    // borrar la calibración táctil guardada y forzar que se repita al
    // reiniciar (más fiable que mantener el dedo pulsado si el táctil
    // aún da lecturas inestables).
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "CAL") {
            storageClearTouchCalibration();
            Serial.println("Reiniciando para recalibrar...");
            delay(300);
            ESP.restart();
        }
    }

    // Bucle principal: interfaz tactil + portal web de configuracion.
    // El conteo real ocurre por interrupcion (counter_sensor.cpp) y
    // el envio a servidor corre en su propia tarea (send_task.cpp).
    uiLoop();
    configWebServerHandle();
    delay(20);
}
