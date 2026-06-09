#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h"

// ================= WIFI =================
const char* ssid = "ZAM_ASUS";
const char* password = "12345678";

// ================= HX711 SENSOR 1 =================
const int LOADCELL1_DOUT_PIN = 16;
const int LOADCELL1_SCK_PIN  = 4;

// ================= HX711 SENSOR 2 =================
const int LOADCELL2_DOUT_PIN = 17;
const int LOADCELL2_SCK_PIN  = 5;

HX711 balanza1;
HX711 balanza2;

// ================= WEB SERVER =================
WebServer server(80);

// ================= PAGINA WEB =================
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
    width:400px;
    margin:auto;
    background:white;
    padding:30px;
    border-radius:15px;
    box-shadow:0px 0px 15px rgba(0,0,0,0.2);
}

h1{
    color:#333;
}

.sensor{
    margin-top:20px;
}

.valor{
    font-size:55px;
    font-weight:bold;
    color:#2196F3;
}

</style>
</head>

<body>

<div class="card">

<h1>Lectura de Peso</h1>

<div class="sensor">
    <h2>Sensor 1</h2>
    <div id="peso1" class="valor">0.00 g</div>
</div>

<div class="sensor">
    <h2>Sensor 2</h2>
    <div id="peso2" class="valor">0.00 g</div>
</div>

</div>

<script>

function actualizarPeso(){

    fetch('/peso')
    .then(response => response.json())
    .then(data => {

        document.getElementById("peso1").innerHTML =
            data.peso1 + " g";

        document.getElementById("peso2").innerHTML =
            data.peso2 + " g";
    });
}

setInterval(actualizarPeso, 500);
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

  float peso1 = balanza1.get_units(5);
  float peso2 = balanza2.get_units(5);

  String json = "{";
  json += "\"peso1\":" + String(peso1, 2) + ",";
  json += "\"peso2\":" + String(peso2, 2);
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {

  Serial.begin(115200);

  // ================= SENSOR 1 =================
  balanza1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  balanza1.set_scale(1811.59);
  balanza1.tare();

  // ================= SENSOR 2 =================
  balanza2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  balanza2.set_scale(1811.59);   // Ajustar al calibrar
  balanza2.tare();

  // ================= WIFI =================
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

  // ================= RUTAS WEB =================
  server.on("/", paginaPrincipal);
  server.on("/peso", enviarPeso);

  server.begin();

  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
}
