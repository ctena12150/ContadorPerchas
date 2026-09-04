# Contador de perchas — ESP32-S3 + ILI9488 (3.5") + XPT2046
## Versión Arduino IDE con TFT_eSPI

## 1. Instalación

1. **Core ESP32**: Archivo → Preferencias → añade esta URL en "Gestor de URLs Adicionales de Tarjetas":
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   Luego Herramientas → Placa → Gestor de tarjetas → instala **esp32 (Espressif Systems)**.

2. **Librerías** (Programa → Incluir Librería → Gestionar Librerías):
   - `TFT_eSPI` (autor: Bodmer)
   - `ArduinoJson` (autor: bblanchon) — versión 7.x

3. **⚠️ Configuración obligatoria de TFT_eSPI** — esta librería se
   configura editando un fichero DENTRO de la propia librería (no en
   el sketch):
   - Ve a `Documentos\Arduino\libraries\TFT_eSPI\`
   - Abre `User_Setup.h`
   - Sustituye su contenido por el de `User_Setup_ContadorPerchas.h`
     (incluido en esta carpeta)
   - Guarda

   Esto define el driver ILI9488, los pines SPI de pantalla/táctil
   (los mismos que ya tenías probados: `MOSI 11, SCLK 12, CS 10, DC 9,
   RST 8, TOUCH_CS 15`) y las velocidades SPI.

   Revisa especialmente `TFT_MISO` (pin 13 puesto por defecto): si tu
   cableado real usa otro pin, o no usas MISO, ajústalo ahí.

4. **Carpeta del sketch**: abre `ContadorPerchas.ino` desde Arduino IDE;
   el resto de ficheros `.h`/`.cpp` de esta misma carpeta aparecerán
   como pestañas automáticamente. No los muevas de sitio ni renombres
   la carpeta (debe llamarse igual que el `.ino`).

5. **Placa**: Herramientas → Placa → **"ESP32S3 Dev Module"**.
   Revisa también en Herramientas:
   - USB CDC On Boot: según prefieras ver el Monitor Serie por USB nativo
   - Partition Scheme: "Default 4MB" (o el que corresponda a tu módulo)
   - PSRAM: actívalo si tu módulo lo trae (N16R8 etc.)

## 2. Cableado

### Pantalla ILI9488 + táctil XPT2046 (SPI, comparten bus)
Ya configurado en `User_Setup_ContadorPerchas.h` (pines 8,9,10,11,12,15).
No se tocan en `config.h`: TFT_eSPI los toma directamente de su propia
configuración.

### Lector de código QR/barras (UART)
- GPIO 17 ← TX del lector
- GPIO 16 → RX del lector (no siempre necesario, según el modelo)
- Configura el lector en modo "salida serie" (no "teclado USB"), y ajusta
  `QR_BAUDRATE` en `config.h` al valor de tu lector (9600 típico).
- Se evitan a propósito GPIO19/20: son el USB nativo del ESP32-S3.

### Sensor de fibra óptica E3X-NA41 (⚠️ atención)
El amplificador **E3X-NA41 se alimenta a 12–24V** y su salida digital
puede trabajar a ese nivel. **El ESP32-S3 NO tolera más de 3.3V en sus GPIO.**

**Nunca conectes la salida del sensor directamente a un GPIO.** Usa uno de estos métodos:
- Optoacoplador (p. ej. **PC817**): el lado de entrada se alimenta desde la
  salida del E3X-NA41 (con su resistencia limitadora), y el lado de salida
  (fototransistor) se conecta entre el GPIO (con pull-up a 3.3V) y GND del ESP32.
- Un módulo comercial de interfaz opto-aislada / level-shifter para PLC.

Pin de entrada por defecto: **GPIO 4** (`PIN_SENSOR` en `config.h`) — cambia
este valor si tu módulo concreto de ESP32-S3 reserva ese pin para otra cosa
(algunos módulos con PSRAM/flash octal usan GPIO26-32 y 33-37 internamente;
GPIO4 suele estar libre, pero conviene comprobarlo en el datasheet de tu
placa concreta).

El debounce por software está en `SENSOR_DEBOUNCE_US` (microsegundos). Las
perchas pasan rápido: mide el ancho real del pulso de tu instalación y ajusta
este valor a un 60-70% de ese ancho mínimo para no perder conteos ni contar
rebotes.

## 3. Primer arranque

1. Al arrancar por primera vez no habrá WiFi configurado: el ESP32 crea un
   punto de acceso `ESP32-Perchas-Config` (contraseña `config1234`).
2. Conéctate a esa red y abre `http://192.168.4.1` en el navegador.
3. Rellena SSID/contraseña WiFi, servidor NTP, endpoint del API y datos de
   Telegram (token del bot y chat_id). Al guardar, el ESP32 se reinicia y
   se conecta a tu red.
4. Vuelve a entrar al portal (ahora en la IP que te asigne tu router — se
   muestra en la pantalla al arrancar) si necesitas cambiar algo más adelante.
5. La primera vez también se lanza automáticamente la **calibración táctil**
   (toca las cruces que aparecen en pantalla). El resultado se guarda en NVS
   y no se vuelve a pedir salvo que borres la memoria flash.
6. **Si la pantalla queda mal calibrada** (el botón no responde donde
   tocas): reinicia el ESP32 y, nada más encender, **mantén el dedo
   apoyado en la pantalla sin soltar durante 1,2 segundos**. Esto fuerza
   una recalibración aunque la guardada estuviera mal — funciona porque
   la simple detección de "hay un toque" no depende de la calibración,
   solo el cálculo de en qué X/Y ha sido. También puedes recalibrar en
   caliente manteniendo pulsado 1,5s el icono **"C"** de la esquina
   superior derecha de la pantalla.

## 4. Funcionamiento (sesión con varios artículos por envío)

- Pantalla en reposo: botón verde **INICIO**.
- Al pulsarlo: arranca una **sesión** (botón pasa a rojo **FIN**), a la
  espera de leer códigos.
- Cada lectura de código por el lector serie:
  - Aparece inmediatamente en la tarjeta **"ARTICULO ACTUAL"**: código,
    contador en vivo y hora de lectura.
  - Si ya había un artículo activo, su conteo se cierra con el valor
    definitivo y pasa a la lista **"Anteriores en esta sesión"**, debajo,
    con scroll (flechas ^/v a la derecha) si hay muchos.
  - El nuevo código pasa a ser el activo, con contador a 0 y su propia
    hora de inicio.
- Cada pulso del sensor de fibra óptica suma al contador del artículo
  activo (si hay uno).
- En la esquina superior derecha de la cabecera aparece siempre
  **"Pendientes: N"**: envíos que aún no se han confirmado contra el
  servidor.
- Al pulsar **FIN**: se cierra el artículo activo, y **todos** los
  artículos leídos en la sesión (n códigos, cada uno con su conteo y
  hora de inicio) se empaquetan en un único **envío** con un
  **ID único** (`envio_id`, p.ej. `EV-20260818T101530-A1B2`). Ese envío
  se encola y el sistema vuelve a reposo (pantalla limpia, lista a cero).
- El envío real lo hace la **tarea FreeRTOS independiente**
  (`send_task.cpp`, en el núcleo 0): si el POST tiene éxito (HTTP 2xx),
  se retira de la cola. Si falla, aparece un aviso en la franja roja de
  la pantalla y se manda un mensaje al bot de Telegram (máx. 1 aviso por
  minuto), y se reintenta automáticamente sin bloquear el conteo.

## 5. Formato enviado al endpoint (POST, JSON)

Cada POST es UN envío completo, con todos los artículos leídos en esa
sesión:

```json
{
  "envio_id": "EV-20260818T101530-A1B2",
  "dispositivo": "Linea1-PuestoA",
  "centro": "1901",
  "creado": "2026-08-18T10:15:30Z",
  "articulos": [
    { "barcode": "1234567890", "start_time": "2026-08-18T09:15:03Z", "count": 214 },
    { "barcode": "9876543210", "start_time": "2026-08-18T09:17:41Z", "count": 87 }
  ]
}
```

Se considera éxito cualquier respuesta HTTP 2xx; en caso contrario el
envío completo permanece en la cola y se reintenta.

## 6. Cosas a revisar/adaptar antes de producción

- `TFT_MISO` en `User_Setup_ContadorPerchas.h` si tu cableado real difiere.
- `PIN_SENSOR`, `PIN_QR_RX/TX` en `config.h` según los pines libres reales de
  tu módulo ESP32-S3 concreto.
- `SENSOR_DEBOUNCE_US` con la velocidad real de las perchas en tu línea.
- Autenticación del endpoint (API key / token) si tu servidor la requiere:
  añadir cabecera en `postRecordToEndpoint()` (`send_task.cpp`).
- Validación del certificado TLS de Telegram en producción (actualmente se
  usa `setInsecure()` para simplificar; puedes fijar la huella/CA si lo
  necesitas).
- Tamaño de `MAX_PENDING_ENVIOS` (`config.h`, colchón de envíos en cola) según
  cuánto tiempo puede estar caída la red sin perder histórico. Cada envío
  puede llevar cuantos artículos se lean en una sesión — no hay límite fijo,
  crece dinámicamente (usa `std::vector`).
