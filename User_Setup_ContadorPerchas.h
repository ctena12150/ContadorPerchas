// ============================================================
//  User_Setup.h para TFT_eSPI — Contador de Perchas (ESP32-S3)
// ============================================================
//  CÓMO USARLO:
//  1. Localiza la carpeta de la librería TFT_eSPI que instalaste,
//     normalmente en:  Documentos\Arduino\libraries\TFT_eSPI
//  2. Abre el fichero  User_Setup.h  que hay dentro de esa carpeta.
//  3. Sustituye TODO su contenido por el de este fichero (o añade
//     estas líneas si prefieres mantener otras que ya tuvieras).
//  4. Guarda y vuelve a compilar el sketch.
// ============================================================

#define USER_SETUP_INFO "ContadorPerchas_ESP32S3"

// --- Controlador de pantalla ---
#define ILI9488_DRIVER

// --- Pines SPI de pantalla (EXACTAMENTE los de tu codigo de referencia) ---
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

// --- Táctil resistivo XPT2046 (comparte reloj y MOSI con la pantalla) ---
// Según tu cableado real: TCK=12 (compartido), TCS=18, TDI=19 (linea de
// retorno de datos del tactil = MISO), TOI=11 (compartido con TFT_MOSI).
#define TFT_MISO 19
#define TOUCH_CS 18

// --- Velocidades SPI ---
#define SPI_FREQUENCY        27000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

// --- Fuentes (activa las que necesites; estas cubren lo habitual) ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
