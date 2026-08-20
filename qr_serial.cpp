#include "globals.h"

static HardwareSerial QRSerial(2); // UART2

static void qrReaderTask(void *param) {
    static char buf[BARCODE_MAX_LEN];
    size_t idx = 0;

    QRSerial.begin(QR_BAUDRATE, SERIAL_8N1, PIN_QR_RX, PIN_QR_TX);

    for (;;) {
        if (QRSerial.available()) {
            idx = 0;
            // El lector NO manda retorno de carro: se considera que el
            // código ha terminado cuando deja de llegar nada nuevo por el
            // puerto durante unos milisegundos (igual que en tu otro
            // programa: leer mientras haya datos, con una pequeña pausa
            // entre lecturas para dar tiempo a que llegue el resto).
            while (QRSerial.available()) {
                char c = (char)QRSerial.read();
                if (c != '\r' && c != '\n' && idx < BARCODE_MAX_LEN - 1) {
                    buf[idx++] = c;
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }

            if (idx > 0) {
                buf[idx] = '\0';
                handleNewBarcode(buf);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void qrReaderTaskStart() {
    xTaskCreatePinnedToCore(
        qrReaderTask,
        "qr_reader",
        4096,
        nullptr,
        2,      // prioridad media-alta: no debe perder caracteres
        nullptr,
        1       // core 1, junto con el resto de la lógica de UI
    );
}
