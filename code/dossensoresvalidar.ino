#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h"

// ================= WIFI =================
const char* ssid = "ZAM_ASUS";
const char* password = "12345678";

// ================= HX711 =================
#define DOUT1 16
#define SCK1  4

#define DOUT2 17
#define SCK2  5

HX711 balanza1;
HX711 balanza2;

// Factores de calibración
float factor1 = 214.3;
float factor2 = 225.0;

// ================= VALIDACION =================
const float UMBRAL_DETECCION = 5.0;
const float TOLERANCIA = 10.0;

// ================= VARIABLES =================
float pesoEntrada = 0;
float pesoSalida = 0;
float diferencia = 0;

float pesoActualEntrada = 0;
float pesoActualSalida = 0;

float pesoMaxEntrada = 0;
float pesoMaxSalida = 0;

bool piezaEntrada = false;
bool piezaSalida = false;

String estado = "ESPERANDO";

int totalPiezas = 0;
int piezasOK = 0;
int piezasError = 0;

unsigned long ultimoMuestreo = 0;

// ================= WEB =================
WebServer server(80);

String paginaHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Conveyor Validation System</title>

<style>
body{
font-family:Arial,sans-serif;
background:#f4f6f9;
padding:20px;
text-align:center;
}

.grid{
display:grid;
grid-template-columns:repeat(auto-fit,minmax(250px,1fr));
gap:20px;
max-width:1100px;
margin:auto;
}

.card{
background:white;
padding:20px;
border-radius:15px;
box-shadow:0px 0px 12px rgba(0,0,0,0.15);
}

.label{
font-size:18px;
color:#666;
}

.value{
font-size:42px;
font-weight:bold;
margin-top:10px;
}

.azul{color:#2196F3;}
.verde{color:#4CAF50;}
.rojo{color:#F44336;}
.naranja{color:#FF9800;}

#estado{
font-size:55px;
font-weight:bold;
}
</style>
</head>

<body>

<h1>Conveyor Validation System</h1>

<div class="grid">

<div class="card">
<div class="label">Entrada (Tiempo Real)</div>
<div id="entrada" class="value azul">0.00 g</div>
</div>

<div class="card">
<div class="label">Salida (Tiempo Real)</div>
<div id="salida" class="value azul">0.00 g</div>
</div>

<div class="card">
<div class="label">Diferencia</div>
<div id="diferencia" class="value naranja">0.00 g</div>
</div>

<div class="card">
<div class="label">Estado</div>
<div id="estado">---</div>
</div>

<div class="card">
<div class="label">Procesadas</div>
<div id="total" class="value">0</div>
</div>

<div class="card">
<div class="label">Aceptadas</div>
<div id="ok" class="value verde">0</div>
</div>

<div class="card">
<div class="label">Rechazadas</div>
<div id="error" class="value rojo">0</div>
</div>

</div>

<script>

function actualizar(){

fetch('/datos')
.then(response => response.json())
.then(data => {

document.getElementById("entrada").innerHTML =
data.entradaLive.toFixed(2) + " g";

document.getElementById("salida").innerHTML =
data.salidaLive.toFixed(2) + " g";

document.getElementById("diferencia").innerHTML =
data.diferencia.toFixed(2) + " g";

document.getElementById("total").innerHTML =
data.total;

document.getElementById("ok").innerHTML =
data.ok;

document.getElementById("error").innerHTML =
data.error;

let est = document.getElementById("estado");

est.innerHTML = data.estado;

if(data.estado == "OK"){
  est.style.color = "#4CAF50";
}
else if(data.estado == "ERROR"){
  est.style.color = "#F44336";
}
else{
  est.style.color = "#000000";
}

});
}

setInterval(actualizar,50);

</script>

</body>
</html>
)rawliteral";

// ================= WEB =================

void paginaPrincipal() {
  server.send(200, "text/html", paginaHTML);
}

void enviarDatos() {

  String json = "{";

  json += "\"entrada\":" + String(pesoEntrada,2) + ",";
  json += "\"salida\":" + String(pesoSalida,2) + ",";
  json += "\"entradaLive\":" + String(pesoActualEntrada,2) + ",";
  json += "\"salidaLive\":" + String(pesoActualSalida,2) + ",";
  json += "\"diferencia\":" + String(diferencia,2) + ",";
  json += "\"estado\":\"" + estado + "\",";
  json += "\"total\":" + String(totalPiezas) + ",";
  json += "\"ok\":" + String(piezasOK) + ",";
  json += "\"error\":" + String(piezasError);

  json += "}";

  server.send(200, "application/json", json);
}

// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  balanza1.begin(DOUT1, SCK1);
  balanza1.set_scale(factor1);
  balanza1.tare();

  balanza2.begin(DOUT2, SCK2);
  balanza2.set_scale(factor2);
  balanza2.tare();

  delay(1000);

  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", paginaPrincipal);
  server.on("/datos", enviarDatos);

  server.begin();
}

// ================= LOOP =================

void loop()
{
  if (millis() - ultimoMuestreo >= 50)
  {
    ultimoMuestreo = millis();

    float p1 = balanza1.get_units(1);
    float p2 = balanza2.get_units(1);

    pesoActualEntrada = p1;
    pesoActualSalida = p2;

    // ===== ENTRADA =====

    if(!piezaEntrada)
    {
      if(p1 > UMBRAL_DETECCION)
      {
        piezaEntrada = true;
        pesoMaxEntrada = p1;
      }
    }
    else
    {
      if(p1 > pesoMaxEntrada)
        pesoMaxEntrada = p1;

      if(p1 < UMBRAL_DETECCION)
      {
        pesoEntrada = pesoMaxEntrada;
        piezaEntrada = false;
      }
    }

    // ===== SALIDA =====

    if(!piezaSalida)
    {
      if(p2 > UMBRAL_DETECCION)
      {
        piezaSalida = true;
        pesoMaxSalida = p2;
      }
    }
    else
    {
      if(p2 > pesoMaxSalida)
        pesoMaxSalida = p2;

      if(p2 < UMBRAL_DETECCION)
      {
        pesoSalida = pesoMaxSalida;
        piezaSalida = false;

        diferencia = abs(pesoEntrada - pesoSalida);

        totalPiezas++;

        if(diferencia <= TOLERANCIA)
        {
          estado = "OK";
          piezasOK++;
        }
        else
        {
          estado = "ERROR";
          piezasError++;
        }
      }
    }
  }

  server.handleClient();
}
