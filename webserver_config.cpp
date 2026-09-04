#include <WebServer.h>
#include "globals.h"

static WebServer server(WEBSERVER_PORT);

// El portal exige login solo si el usuario ha configurado ALGUNA
// credencial desde la propia web. En estado de fábrica (ambos campos
// vacíos) no se pide nada, tal y como pediste.
static bool authRequired() {
  return strlen(appConfig.web_user) > 0 || strlen(appConfig.web_pass) > 0;
}

// Devuelve true si se puede continuar sirviendo la petición.
// Si hace falta login y no se ha superado, ya responde 401 y devuelve false.
static bool checkAuth() {
  if (!authRequired()) return true;
  if (server.authenticate(appConfig.web_user, appConfig.web_pass)) return true;
  server.requestAuthentication(BASIC_AUTH, "Contador de Perchas");
  delay(500);  // pequeña penalización ante intentos fallidos (freno básico a fuerza bruta)
  return false;
}

static String htmlPage() {
  String html;
  html.reserve(3600);
  html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuracion Contador de Perchas</title>";
  html += "<style>body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px}";
  html += "label{display:block;margin-top:12px;font-weight:bold}";
  html += "input{width:100%;padding:8px;box-sizing:border-box}";
  html += "button{margin-top:20px;padding:10px 20px;width:100%}";
  html += "small{color:#666}";
  html += "h2{border-bottom:1px solid #ccc;padding-bottom:4px;margin-top:24px}</style></head><body>";
  html += "<h1>Configuracion</h1><form method='POST' action='/save'>";

  html += "<h2>WiFi</h2>";
  html += "<label>SSID</label><input name='ssid' value='" + String(appConfig.wifi_ssid) + "'>";
  html += "<label>Contrasena</label><input name='pass' type='password' autocomplete='new-password' placeholder='(sin cambios)'>";

  html += "<h2>Dispositivo</h2>";
  html += "<label>Identificador de dispositivo</label><input name='device' value='" + String(appConfig.device_id) + "' placeholder='p.ej. Linea1-PuestoA'>";
  html += "<label>Centro</label><input name='centro' value='" + String(appConfig.centro_id) + "' placeholder='p.ej. 5001'>";
  html += "<small>Ambos se envian con cada registro al servidor para saber de que centro y maquina viene.</small>";
  

  html += "<h2>Hora / NTP</h2>";
  html += "<label>Servidor NTP</label><input name='ntp' value='" + String(appConfig.ntp_server) + "'>";
  html += "<label>Offset GMT (segundos)</label><input name='gmt' value='" + String(appConfig.gmt_offset_sec) + "'>";
  html += "<label>Offset horario verano (segundos)</label><input name='dst' value='" + String(appConfig.dst_offset_sec) + "'>";

  html += "<h2>Servidor / API</h2>";
  html += "<label>Endpoint (URL)</label><input name='endpoint' value='" + String(appConfig.api_endpoint) + "'>";

  html += "<h2>Telegram</h2>";
  html += "<label>Bot Token</label><input name='tgtoken' type='password' autocomplete='new-password' placeholder='(sin cambios)'>";
  html += "<label>Chat ID</label><input name='tgchat' value='" + String(appConfig.tg_chat_id) + "'>";

  html += "<h2>Sensor</h2>";
  html += "<label>Debounce (microsegundos)</label><input name='debounce' type='number' min='0' step='1' value='" + String(appConfig.sensor_debounce_us) + "'>";
  html += "<small>Mide el ancho real del pulso de tu instalacion y ajusta este valor a un 60-70% de ese ancho minimo para no perder conteos ni contar rebotes.</small>";

  html += "<h2>Seguridad del portal web</h2>";
  html += "<label>Usuario</label><input name='webuser' value='" + String(appConfig.web_user) + "'>";
  html += "<label>Contrasena</label><input name='webpass' type='password' autocomplete='new-password' placeholder='(sin cambios)'>";
  html += "<small>Si dejas ambos campos vacios, el portal no pedira usuario/contrasena al acceder. Rellena cualquiera de los dos para activar el login (se pedira en el siguiente acceso).</small>";

  html += "<button type='submit'>Guardar y reiniciar</button></form>";
  html += "<p>IP actual: " + WiFi.localIP().toString() + "</p>";
  html += "</body></html>";
  return html;
}

static void handleRoot() {
  if (!checkAuth()) return;
  server.send(200, "text/html", htmlPage());
}

static void handleSave() {
  if (!checkAuth()) return;

  if (server.hasArg("ssid")) server.arg("ssid").toCharArray(appConfig.wifi_ssid, sizeof(appConfig.wifi_ssid));
  // Contrasenas: si se deja el campo vacio, se conserva la guardada
  // (evita que aparezcan en el HTML y evita borrados accidentales).
  if (server.hasArg("pass") && server.arg("pass").length() > 0)
    server.arg("pass").toCharArray(appConfig.wifi_pass, sizeof(appConfig.wifi_pass));

  if (server.hasArg("ntp")) server.arg("ntp").toCharArray(appConfig.ntp_server, sizeof(appConfig.ntp_server));
  if (server.hasArg("gmt")) appConfig.gmt_offset_sec = server.arg("gmt").toInt();
  if (server.hasArg("dst")) appConfig.dst_offset_sec = server.arg("dst").toInt();
  if (server.hasArg("endpoint")) server.arg("endpoint").toCharArray(appConfig.api_endpoint, sizeof(appConfig.api_endpoint));

  if (server.hasArg("tgtoken") && server.arg("tgtoken").length() > 0)
    server.arg("tgtoken").toCharArray(appConfig.tg_bot_token, sizeof(appConfig.tg_bot_token));
  if (server.hasArg("tgchat")) server.arg("tgchat").toCharArray(appConfig.tg_chat_id, sizeof(appConfig.tg_chat_id));

  if (server.hasArg("debounce")) {
    long v = server.arg("debounce").toInt();
    if (v < 0) v = 0;
    appConfig.sensor_debounce_us = (uint32_t)v;
  }

  if (server.hasArg("webuser")) server.arg("webuser").toCharArray(appConfig.web_user, sizeof(appConfig.web_user));
  if (server.hasArg("webpass") && server.arg("webpass").length() > 0)
    server.arg("webpass").toCharArray(appConfig.web_pass, sizeof(appConfig.web_pass));


  if (server.hasArg("device")) server.arg("device").toCharArray(appConfig.device_id, sizeof(appConfig.device_id));
  if (server.hasArg("centro")) server.arg("centro").toCharArray(appConfig.centro_id, sizeof(appConfig.centro_id));

  storageSave(appConfig);

  server.send(200, "text/html",
              "<html><body><p>Configuracion guardada. Reiniciando...</p></body></html>");
  delay(1000);
  ESP.restart();
}

void configWebServerStart() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void configWebServerHandle() {
  server.handleClient();
}
