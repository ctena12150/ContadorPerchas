#include "globals.h"

// ============================================================
//  Paleta de colores (RGB565)
// ============================================================
#define COL_BG      TFT_BLACK
#define COL_HEADER  0x10A2
#define COL_CARD    0x18C3
#define COL_BORDER  0x3186
#define COL_ACCENT  0x07FF
#define COL_TEXT    TFT_WHITE
#define COL_MUTED   0x8410
#define COL_OK      0x07E0
#define COL_WARN    0xFD20
#define COL_ERR     TFT_RED

// ============================================================
//  Layout (pantalla 480x320, rotation 1)
// ============================================================
#define SCR_W 480
#define SCR_H 320

#define HDR_Y 0
#define HDR_H 34

#define CARD_Y 38
#define CARD_H 90

#define LIST_Y (CARD_Y + CARD_H + 8)   // 136
#define LIST_H 100                      // termina en 236
#define LIST_ROW_H 26
#define LIST_VISIBLE_ROWS 3

#define ERR_Y 240
#define ERR_H 26

#define BTN_W 220
#define BTN_H 42
#define BTN_X ((SCR_W - BTN_W) / 2)
#define BTN_Y 272

#define ARROW_X (SCR_W - 30)
#define ARROW_W 26
#define ARROW_H 34
#define ARROW_UP_Y (LIST_Y + 18)
#define ARROW_DN_Y (LIST_Y + LIST_H - 40)

#define CAL_ICON_X (SCR_W - 30)
#define CAL_ICON_Y 4
#define CAL_ICON_W 26
#define CAL_ICON_H 26

// ============================================================
//  Estado interno de dibujo / interacción
// ============================================================
static uint32_t lastTouchMillis = 0;
static bool lastHasError = false;
static char lastErrorShown[128] = "";
static size_t oldOffset = 0; // desplazamiento (scroll) en la lista de artículos anteriores
static uint32_t calIconPressStart = 0;

static char lastCardBarcode[BARCODE_MAX_LEN] = "\x01"; // valor imposible -> fuerza 1er dibujo
static uint32_t lastCardCount = 0xFFFFFFFF;
static time_t lastCardStart = (time_t)-1;
static size_t lastSessionCountShown = 0xFFFFFFFF;
static size_t lastOldOffsetShown = 0xFFFFFFFF;

static void formatTimeShort(time_t t, char *out, size_t outSize) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(out, outSize, "%H:%M:%S", &tmv);
}

// ============================================================
//  Cabecera: título, estado (punto de color) e indicador de pendientes
// ============================================================
static void drawHeader(bool force) {
    static SystemState lastState = (SystemState)0xFF;
    static size_t lastPending = 0xFFFFFFFF;

    if (force) {
        tft.fillRect(0, HDR_Y, SCR_W, HDR_H, COL_HEADER);
        tft.setTextColor(COL_TEXT, COL_HEADER);
        tft.setTextSize(2);
        tft.setCursor(8, 8);
        tft.print("CONTADOR DE PERCHAS");

        // Icono de recalibración (mantener pulsado 1.5s para recalibrar)
        tft.fillRoundRect(CAL_ICON_X, CAL_ICON_Y, CAL_ICON_W, CAL_ICON_H, 4, COL_CARD);
        tft.drawRoundRect(CAL_ICON_X, CAL_ICON_Y, CAL_ICON_W, CAL_ICON_H, 4, COL_BORDER);
        tft.setTextColor(COL_MUTED, COL_CARD);
        tft.setTextSize(1);
        tft.setCursor(CAL_ICON_X + 9, CAL_ICON_Y + 9);
        tft.print("C");

        lastState = (SystemState)0xFF;
        lastPending = 0xFFFFFFFF;
    }

    if (force || systemState != lastState) {
        uint16_t dotColor = (systemState == STATE_RUNNING) ? COL_WARN : COL_OK;
        tft.fillRect(258, 10, 16, 16, COL_HEADER);
        tft.fillCircle(266, 18, 7, dotColor);
        lastState = systemState;
    }

    size_t pending = pendingEnvioCount();
    if (force || pending != lastPending) {
        tft.fillRect(290, 4, 150, HDR_H - 8, COL_HEADER);
        tft.setTextColor(COL_MUTED, COL_HEADER);
        tft.setTextSize(1);
        char buf[24];
        snprintf(buf, sizeof(buf), "Pendientes: %u", (unsigned)pending);
        tft.setCursor(290, 13);
        tft.print(buf);
        lastPending = pending;
    }
}

// ============================================================
//  Tarjeta del artículo activo (el que está sumando perchas ahora)
// ============================================================
static void drawCard(bool force) {
    if (force) {
        tft.fillRoundRect(8, CARD_Y, SCR_W - 16, CARD_H, 8, COL_CARD);
        tft.drawRoundRect(8, CARD_Y, SCR_W - 16, CARD_H, 8, COL_BORDER);
        tft.setTextColor(COL_MUTED, COL_CARD);
        tft.setTextSize(1);
        tft.setCursor(16, CARD_Y + 6);
        tft.print("ARTICULO ACTUAL");
    }

    size_t n = sessionArticleCount();
    ArticleRecord active;
    bool has = (n > 0) && getSessionArticle(n - 1, active);
    uint32_t liveCount = has ? getActiveLiveCount() : 0;

    char barcodeToShow[BARCODE_MAX_LEN];
    strncpy(barcodeToShow, has ? active.barcode : "-", sizeof(barcodeToShow) - 1);
    barcodeToShow[sizeof(barcodeToShow) - 1] = '\0';

    if (force || strcmp(barcodeToShow, lastCardBarcode) != 0) {
        tft.fillRect(16, CARD_Y + 20, SCR_W - 32, 24, COL_CARD);
        tft.setTextColor(COL_ACCENT, COL_CARD);
        tft.setTextSize(2);
        tft.setCursor(16, CARD_Y + 20);
        tft.print(barcodeToShow);
        strncpy(lastCardBarcode, barcodeToShow, sizeof(lastCardBarcode));
    }

    if (force || liveCount != lastCardCount) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Cont: %-6lu", (unsigned long)liveCount);
        tft.fillRect(266, CARD_Y + 48, 200, 28, COL_CARD);
        tft.setTextColor(COL_TEXT, COL_CARD);
        tft.setTextSize(3);
        tft.setCursor(266, CARD_Y + 48);
        tft.print(buf);
        lastCardCount = liveCount;
    }

    time_t st = has ? active.startTime : (time_t)0;
    if (force || st != lastCardStart) {
        char tbuf[10] = "--:--:--";
        if (has) formatTimeShort(st, tbuf, sizeof(tbuf));
        tft.fillRect(16, CARD_Y + 52, 240, 20, COL_CARD);
        tft.setTextColor(COL_MUTED, COL_CARD);
        tft.setTextSize(1);
        tft.setCursor(16, CARD_Y + 56);
        tft.print("Inicio: ");
        tft.print(tbuf);
        lastCardStart = st;
    }
}

// ============================================================
//  Lista de artículos ya leídos en esta sesión (con scroll)
// ============================================================
static void drawOldRow(int rowIndex, bool hasItem, const ArticleRecord &rec) {
    int y = LIST_Y + 22 + rowIndex * LIST_ROW_H;
    uint16_t rowBg = (rowIndex % 2 == 0) ? COL_CARD : COL_BG;
    tft.fillRect(16, y, 420, LIST_ROW_H - 2, rowBg);
    if (!hasItem) return;

    char tbuf[10];
    formatTimeShort(rec.startTime, tbuf, sizeof(tbuf));

    tft.setTextColor(COL_TEXT, rowBg);
    tft.setTextSize(1);
    tft.setCursor(20, y + 6);
    char line[72];
    snprintf(line, sizeof(line), "%-22s x%-6lu %s", rec.barcode, (unsigned long)rec.count, tbuf);
    tft.print(line);
}

static void drawList(bool force) {
    size_t total = sessionArticleCount();
    size_t oldCount = (total > 0) ? (total - 1) : 0;

    size_t maxOffset = (oldCount > LIST_VISIBLE_ROWS) ? (oldCount - LIST_VISIBLE_ROWS) : 0;
    if (oldOffset > maxOffset) oldOffset = maxOffset;

    if (force) {
        tft.fillRect(0, LIST_Y, SCR_W, LIST_H, COL_BG);

        tft.fillRoundRect(ARROW_X, ARROW_UP_Y, ARROW_W, ARROW_H, 4, COL_CARD);
        tft.drawRoundRect(ARROW_X, ARROW_UP_Y, ARROW_W, ARROW_H, 4, COL_BORDER);
        tft.setTextColor(COL_TEXT, COL_CARD);
        tft.setTextSize(2);
        tft.setCursor(ARROW_X + 7, ARROW_UP_Y + 8);
        tft.print("^");

        tft.fillRoundRect(ARROW_X, ARROW_DN_Y, ARROW_W, ARROW_H, 4, COL_CARD);
        tft.drawRoundRect(ARROW_X, ARROW_DN_Y, ARROW_W, ARROW_H, 4, COL_BORDER);
        tft.setCursor(ARROW_X + 7, ARROW_DN_Y + 8);
        tft.print("v");

        lastSessionCountShown = 0xFFFFFFFF;
        lastOldOffsetShown = 0xFFFFFFFF;
    }

    if (!force && total == lastSessionCountShown && oldOffset == lastOldOffsetShown) return;

    tft.fillRect(16, LIST_Y, 380, 18, COL_BG);
    tft.setTextColor(COL_MUTED, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(16, LIST_Y + 4);
    char lbl[48];
    snprintf(lbl, sizeof(lbl), "Anteriores en esta sesion (%u)", (unsigned)oldCount);
    tft.print(lbl);

    for (int r = 0; r < LIST_VISIBLE_ROWS; r++) {
        size_t k = oldOffset + r;
        bool hasItem = (k < oldCount);
        ArticleRecord rec;
        if (hasItem) {
            // el índice count-1 es el activo; el resto son "anteriores",
            // recorridos del más reciente al más antiguo.
            size_t idx = total - 2 - k;
            hasItem = getSessionArticle(idx, rec);
        }
        drawOldRow(r, hasItem, rec);
    }

    lastSessionCountShown = total;
    lastOldOffsetShown = oldOffset;
}

// ============================================================
//  Banner de error (aparece solo si hay un fallo de envío)
// ============================================================
static void drawErrorBox(bool force) {
    bool err;
    char msg[128];
    if (xSemaphoreTake(errorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        err = hasError;
        strncpy(msg, lastErrorMsg, sizeof(msg));
        xSemaphoreGive(errorMutex);
    } else {
        return;
    }

    if (!force && err == lastHasError && strcmp(msg, lastErrorShown) == 0) return;
    lastHasError = err;
    strncpy(lastErrorShown, msg, sizeof(lastErrorShown));

    tft.fillRect(0, ERR_Y, SCR_W, ERR_H, err ? COL_ERR : COL_BG);
    if (err) {
        tft.setTextColor(TFT_WHITE, COL_ERR);
        tft.setTextSize(1);
        tft.setCursor(8, ERR_Y + 8);
        tft.print(msg);
    }
}

// ============================================================
//  Botón principal INICIO / FIN
// ============================================================
static void drawButton(bool force) {
    static SystemState lastBtnState = (SystemState)0xFF;
    if (!force && systemState == lastBtnState) return;

    uint16_t col = (systemState == STATE_RUNNING) ? COL_ERR : COL_OK;
    const char *label = (systemState == STATE_RUNNING) ? "FIN" : "INICIO";

    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, col);
    tft.drawRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, COL_TEXT);
    tft.setTextColor(COL_TEXT, col);
    tft.setTextSize(3);
    int textW = strlen(label) * 18;
    tft.setCursor(BTN_X + (BTN_W - textW) / 2, BTN_Y + (BTN_H - 24) / 2);
    tft.print(label);

    lastBtnState = systemState;
}

// ============================================================
//  Redibujo completo / incremental
// ============================================================
static void drawAll(bool force) {
    drawHeader(force);
    drawCard(force);
    drawList(force);
    drawErrorBox(force);
    drawButton(force);
}

// ============================================================
//  Táctil
// ============================================================
static void handleTouch() {
    uint16_t x, y;
    bool touched = tft.getTouch(&x, &y);

    // --- DEBUG: imprime lecturas crudas de tactil (quitar cuando funcione bien) ---
    static bool lastTouchedDbg = false;
    static uint32_t lastDbgPrint = 0;
    if (touched && (millis() - lastDbgPrint > 150)) {
        Serial.printf("[TOUCH] x=%u y=%u\n", x, y);
        lastDbgPrint = millis();
    }
    if (touched != lastTouchedDbg) {
        Serial.printf("[TOUCH] %s\n", touched ? "INICIO toque" : "FIN toque");
        lastTouchedDbg = touched;
    }
    // --- FIN DEBUG ---

    // Icono de recalibración: mantener pulsado 1.5s
    bool onCalIcon = touched && x >= CAL_ICON_X && x <= CAL_ICON_X + CAL_ICON_W &&
                      y >= CAL_ICON_Y && y <= CAL_ICON_Y + CAL_ICON_H;
    if (onCalIcon) {
        if (calIconPressStart == 0) calIconPressStart = millis();
        else if (millis() - calIconPressStart > 1500) {
            calIconPressStart = 0;
            uiRunCalibration();
            drawAll(true);
            return;
        }
    } else {
        calIconPressStart = 0;
    }

    if (!touched) return;

    uint32_t now = millis();
    if (now - lastTouchMillis < 300) return;

    // Flechas de scroll de la lista
    if (x >= ARROW_X && x <= ARROW_X + ARROW_W) {
        if (y >= ARROW_UP_Y && y <= ARROW_UP_Y + ARROW_H) {
            if (oldOffset > 0) oldOffset--;
            lastTouchMillis = now;
            return;
        }
        if (y >= ARROW_DN_Y && y <= ARROW_DN_Y + ARROW_H) {
            oldOffset++; // se recorta automáticamente en drawList()
            lastTouchMillis = now;
            return;
        }
    }

    // Botón INICIO / FIN
    if (x >= BTN_X && x <= BTN_X + BTN_W && y >= BTN_Y && y <= BTN_Y + BTN_H) {
        lastTouchMillis = now;
        if (systemState == STATE_IDLE) {
            startSession();
            oldOffset = 0;
        } else {
            endSession();
        }
    }
}

// ============================================================
//  Calibración
// ============================================================
void uiRunCalibration() {
    uint16_t calData[5];
    Serial.println("[CAL] Iniciando calibracion...");
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_TEXT);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Toque las cruces para calibrar");
    tft.calibrateTouch(calData, TFT_WHITE, TFT_RED, 15);
    Serial.println("[CAL] Calibracion terminada, guardando...");
    storageSaveTouchCalibration(calData);
}

// ============================================================
//  Inicio / bucle
// ============================================================
void uiInit() {
    uint16_t calData[5];
    bool valid = false;
    storageLoadTouchCalibration(calData, valid);

    // Si mantienes el dedo pulsado en la pantalla al arrancar (>1.2s),
    // se fuerza una recalibración aunque ya hubiera una guardada. Esto
    // funciona incluso si la calibración anterior era mala, porque la
    // DETECCIÓN de toque no depende de la calibración (solo la posición
    // X/Y calculada sí depende de ella).
    bool forceRecal = false;
    uint32_t touchStart = 0;
    for (int i = 0; i < 20; i++) {
        uint16_t tx, ty;
        if (tft.getTouch(&tx, &ty)) {
            if (touchStart == 0) touchStart = millis();
            if (millis() - touchStart > 1200) { forceRecal = true; break; }
        } else {
            touchStart = 0;
        }
        delay(75);
    }

    if (!valid || forceRecal) {
        uiRunCalibration();
    } else {
        tft.setTouch(calData);
    }

    tft.fillScreen(COL_BG);
    drawAll(true);
}

void uiLoop() {
    handleTouch();
    drawAll(false);
}

void uiSetError(const String &msg) {
    if (xSemaphoreTake(errorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        hasError = true;
        msg.toCharArray(lastErrorMsg, sizeof(lastErrorMsg));
        xSemaphoreGive(errorMutex);
    }
}

void uiClearError() {
    if (xSemaphoreTake(errorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        hasError = false;
        lastErrorMsg[0] = '\0';
        xSemaphoreGive(errorMutex);
    }
}
