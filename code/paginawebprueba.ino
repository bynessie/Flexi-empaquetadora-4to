#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h"

// ================= WIFI =================
const char* ssid = "ZAM_ASUS";
const char* password = "12345678";

// ================= HX711 =================
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN  = 4;

HX711 balanza;

// ================= WEB SERVER =================
WebServer server(80);

// Página web
String paginaHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Báscula ESP32</title>

<style>

body{
    font-family: Arial, sans-serif;
    text-align:center;
    background:#f5f5f5;
    margin-top:60px;
}

.card{
    width:350px;
    margin:auto;
    background:white;
    padding:30px;
    border-radius:15px;
    box-shadow:0px 0px 15px rgba(0,0,0,0.2);
}

h1{
    color:#333;
}

#peso{
    font-size:65px;
    font-weight:bold;
    color:#2196F3;
}

</style>
</head>

<body>

<div class="card">
<h1>Peso Actual</h1>

<div id="peso">0.00 g</div>
</div>

<script>

function actualizarPeso(){

    fetch('/peso')
    .then(response => response.text())
    .then(data => {
        document.getElementById("peso").innerHTML = data + " g";
    });

}

setInterval(actualizarPeso,500);
actualizarPeso();

</script>

</body>
</html>
)rawliteral";

// ================= FUNCIONES =================

void paginaPrincipal() {
  server.send(200, "text/html", paginaHTML);
}

void enviarPeso() {

  float peso = balanza.get_units(5);

  server.send(200, "text/plain", String(peso, 2));
}

void setup() {

  Serial.begin(115200);

  // HX711
  balanza.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  balanza.set_scale(1811.59);
  balanza.tare();

  // WiFi
  WiFi.begin(ssid, password);

  Serial.print("Conectando a WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", paginaPrincipal);
  server.on("/peso", enviarPeso);

  server.begin();

  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
}
