#include <WebServer.h>
#include "globals.h"

static WebServer server(WEBSERVER_PORT);

static String htmlPage() {
    String html;
    html.reserve(3000);
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Configuracion Contador de Perchas</title>";
    html += "<style>body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px}";
    html += "label{display:block;margin-top:12px;font-weight:bold}";
    html += "input{width:100%;padding:8px;box-sizing:border-box}";
    html += "button{margin-top:20px;padding:10px 20px;width:100%}";
    html += "h2{border-bottom:1px solid #ccc;padding-bottom:4px;margin-top:24px}</style></head><body>";
    html += "<h1>Configuracion</h1><form method='POST' action='/save'>";

    html += "<h2>WiFi</h2>";
    html += "<label>SSID</label><input name='ssid' value='" + String(appConfig.wifi_ssid) + "'>";
    html += "<label>Contrasena</label><input name='pass' type='password' value='" + String(appConfig.wifi_pass) + "'>";

    html += "<h2>Hora / NTP</h2>";
    html += "<label>Servidor NTP</label><input name='ntp' value='" + String(appConfig.ntp_server) + "'>";
    html += "<label>Offset GMT (segundos)</label><input name='gmt' value='" + String(appConfig.gmt_offset_sec) + "'>";
    html += "<label>Offset horario verano (segundos)</label><input name='dst' value='" + String(appConfig.dst_offset_sec) + "'>";

    html += "<h2>Servidor / API</h2>";
    html += "<label>Endpoint (URL)</label><input name='endpoint' value='" + String(appConfig.api_endpoint) + "'>";

    html += "<h2>Telegram</h2>";
    html += "<label>Bot Token</label><input name='tgtoken' value='" + String(appConfig.tg_bot_token) + "'>";
    html += "<label>Chat ID</label><input name='tgchat' value='" + String(appConfig.tg_chat_id) + "'>";

    html += "<button type='submit'>Guardar y reiniciar</button></form>";
    html += "<p>IP actual: " + WiFi.localIP().toString() + "</p>";
    html += "</body></html>";
    return html;
}

static void handleRoot() {
    server.send(200, "text/html", htmlPage());
}

static void handleSave() {
    if (server.hasArg("ssid"))     server.arg("ssid").toCharArray(appConfig.wifi_ssid, sizeof(appConfig.wifi_ssid));
    if (server.hasArg("pass"))     server.arg("pass").toCharArray(appConfig.wifi_pass, sizeof(appConfig.wifi_pass));
    if (server.hasArg("ntp"))      server.arg("ntp").toCharArray(appConfig.ntp_server, sizeof(appConfig.ntp_server));
    if (server.hasArg("gmt"))      appConfig.gmt_offset_sec = server.arg("gmt").toInt();
    if (server.hasArg("dst"))      appConfig.dst_offset_sec = server.arg("dst").toInt();
    if (server.hasArg("endpoint")) server.arg("endpoint").toCharArray(appConfig.api_endpoint, sizeof(appConfig.api_endpoint));
    if (server.hasArg("tgtoken"))  server.arg("tgtoken").toCharArray(appConfig.tg_bot_token, sizeof(appConfig.tg_bot_token));
    if (server.hasArg("tgchat"))   server.arg("tgchat").toCharArray(appConfig.tg_chat_id, sizeof(appConfig.tg_chat_id));

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
