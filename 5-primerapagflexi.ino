#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h> // REQUISITO: Instalar "WebSockets" de Markus Sattler desde el gestor de librerías
#include "HX711.h"

// ================= CONFIGURACIÓN WIFI =================
const char* ssid = "ZAM_ASUS";
const char* password = "12345678";

// ================= CONFIGURACIÓN HX711 =================
#define DOUT1 16
#define SCK1  4

#define DOUT2 17
#define SCK2  5

HX711 balanza1;
HX711 balanza2;

// Factores de calibración (Ajusta según tus celdas)
float factor1 = 214.3;
float factor2 = 225.0;

// ================= PARÁMETROS DE VALIDACIÓN =================
const float UMBRAL_DETECCION = 5.0;  // Peso mínimo para detectar un calzado (en gramos)
const float TOLERANCIA = 10.0;       // Tolerancia máxima permitida entre Entrada y Salida

// ================= VARIABLES DE CONTROL =================
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
unsigned long ultimoEnvioWS = 0;

// ================= SERVIDORES WEB =================
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); // Puerto 81 dedicado a WebSockets

// ================= INTERFAZ WEB SIMULADA (PROGMEM) =================
const char paginaHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Conveyor System - Calzado</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', sans-serif; }
        body { background: linear-gradient(135deg, #0f172a, #1e293b); color: white; padding: 20px; min-height: 100vh; }
        
        header { text-align: center; margin-bottom: 25px; }
        h1 { font-size: 2.2rem; color: #f8fafc; }
        .sub { color: #94a3b8; font-size: 1rem; margin-top: 5px; }

        .dashboard { display: grid; grid-template-columns: 3fr 1fr; gap: 20px; max-width: 1300px; margin: auto; }
        .left-panel { display: flex; flex-direction: column; gap: 20px; }
        
        .conveyor-container { background: #111827; border-radius: 20px; padding: 30px; border: 2px solid #334155; position: relative; }
        .conveyor-belt { height: 80px; background: #334155; border-radius: 40px; position: relative; display: flex; align-items: center; overflow: hidden; border: 4px solid #475569; }
        
        .divider { position: absolute; top: 0; width: 4px; height: 100%; background: #1e293b; box-shadow: 0 0 5px rgba(0,0,0,0.5); }
        .div-1 { left: 25%; } .div-2 { left: 50%; } .div-3 { left: 75%; }

        .zone-label { position: absolute; font-size: 0.75rem; font-weight: bold; text-transform: uppercase; letter-spacing: 1px; color: #94a3b8; top: 5px; }
        .z-in { left: 40px; } .z-out { right: 140px; } .z-seal { right: 25px; }

        .shoe-box { width: 55px; height: 45px; background: #e2e8f0; border-radius: 6px; position: absolute; display: flex; align-items: center; justify-content: center; font-size: 1.5rem; transition: left 0.4s ease-out; border: 2px solid #cbd5e1; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        
        .sealer-machine { position: absolute; right: 20px; top: 15px; width: 70px; height: 110px; background: #475569; border-radius: 10px; border: 2px solid #64748b; display: flex; flex-direction: column; align-items: center; justify-content: center; font-size: 0.8rem; text-align: center; font-weight: bold; }
        .sealer-piston { width: 100%; height: 15px; background: #ef4444; position: absolute; top: 0; transition: transform 0.2s; }
        .sealing .sealer-piston { transform: translateY(35px); background: #22c55e; }

        .grid-stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .card { background: #1e293b; border-radius: 15px; padding: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.2); border: 1px solid #334155; }
        .label { font-size: 0.85rem; color: #94a3b8; text-transform: uppercase; font-weight: 600; }
        .value { font-size: 1.8rem; font-weight: bold; margin-top: 5px; }
        
        .azul { color: #38bdf8; } .naranja { color: #f59e0b; } .verde { color: #22c55e; } .rojo { color: #ef4444; }

        .tower-card { display: flex; flex-direction: column; align-items: center; justify-content: center; background: #1e293b; border-radius: 15px; padding: 20px; border: 1px solid #334155; height: 100%; }
        .light-tower { width: 50px; background: #111827; padding: 10px; border-radius: 10px; display: flex; flex-direction: column; gap: 8px; border: 3px solid #475569; }
        .light { width: 30px; height: 30px; border-radius: 50%; background: #2a2a2a; opacity: 0.15; transition: all 0.2s; box-shadow: inset 0 0 10px rgba(0,0,0,0.8); }
        .light.active.green { background: #22c55e; opacity: 1; box-shadow: 0 0 20px #22c55e; }
        .light.active.yellow { background: #eab308; opacity: 1; box-shadow: 0 0 20px #eab308; }
        .light.active.red { background: #ef4444; opacity: 1; box-shadow: 0 0 20px #ef4444; }
        
        .ticket-box { background: #f8fafc; color: #0f172a; padding: 15px; border-radius: 10px; font-family: 'Courier New', monospace; font-size: 0.8rem; margin-top: 15px; border-left: 5px solid #38bdf8; display: none; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    </style>
</head>
<body>

    <header>
        <h1>📦 Sistema Automatizado de Calzado</h1>
        <div class="sub">Monitoreo del Conveyor, Control de Calidad y Sellado</div>
    </header>

    <div class="dashboard">
        <div class="left-panel">
            <div class="conveyor-container">
                <div class="zone-label z-in">Balanza Entrada</div>
                <div class="zone-label z-out">Balanza Salida</div>
                <div class="zone-label z-seal">Sellado</div>

                <div class="conveyor-belt" id="belt">
                    <div class="divider div-1"></div>
                    <div class="divider div-2"></div>
                    <div class="divider div-3"></div>
                    <div class="shoe-box" id="shoeBox" style="left: -60px;">👟</div>
                </div>

                <div class="sealer-machine" id="sealer">
                    <div class="sealer-piston"></div>
                    <span style="margin-top:20px; z-index:1;">SELLADO</span>
                </div>
            </div>

            <div class="grid-stats">
                <div class="card"><div class="label">⚖️ Peso Entrada</div><div id="entrada" class="value azul">0.0 g</div></div>
                <div class="card"><div class="label">⚖️ Peso Salida</div><div id="salida" class="value azul">0.0 g</div></div>
                <div class="card"><div class="label">📏 Diferencia</div><div id="diferencia" class="value naranja">0.0 g</div></div>
                <div class="card"><div class="label">🆔 Identificador Lote</div><div class="value">LOTE-2026A</div></div>
                <div class="card"><div class="label">📦 Unidades Procesadas</div><div id="total" class="value">0</div></div>
                <div class="card"><div class="label">✅ Empaques OK</div><div id="ok" class="value verde">0</div></div>
                <div class="card"><div class="label">❌ Empaques Bloqueados</div><div id="error" class="value rojo">0</div></div>
            </div>

            <div class="ticket-box" id="ticket">
                <div>====== REGISTRO DE EMPAQUE ======</div>
                <div>LOTE: LOTE-2026A</div>
                <div id="tk-p1">PESO ENTRADA: 0.0g</div>
                <div id="tk-p2">PESO SALIDA: 0.0g</div>
                <div id="tk-dif">DIFERENCIA: 0.0g</div>
                <div id="tk-status">ESTADO: -</div>
                <div>==================================</div>
            </div>
        </div>

        <div class="right-panel">
            <div class="tower-card">
                <div class="label" style="margin-bottom: 15px;">Torre de Luces</div>
                <div class="light-tower">
                    <div class="light red" id="lightRed"></div>
                    <div class="light yellow" id="lightYellow"></div>
                    <div class="light green" id="lightGreen"></div>
                </div>
                <div id="estadoText" style="margin-top: 20px; font-weight: bold; font-size: 1.2rem; text-align:center;">ESPERANDO</div>
            </div>
        </div>
    </div>

    <script>
        let connection = new WebSocket('ws://' + location.hostname + ':81/');
        let totalAnterior = 0;

        connection.onmessage = function(e) {
            let data = JSON.parse(e.data);
            
            document.getElementById("entrada").textContent = data.entradaLive.toFixed(1) + " g";
            document.getElementById("salida").textContent = data.salidaLive.toFixed(1) + " g";
            document.getElementById("diferencia").textContent = data.diferencia.toFixed(1) + " g";
            document.getElementById("total").textContent = data.total;
            document.getElementById("ok").textContent = data.ok;
            document.getElementById("error").textContent = data.error;

            let shoe = document.getElementById("shoeBox");
            let sealer = document.getElementById("sealer");

            if (data.entradaLive > 5) {
                shoe.style.left = "40px"; 
                actualizarLuces("yellow", "VERIFICANDO ENTRADA");
            } 
            else if (data.salidaLive > 5) {
                shoe.style.left = "630px"; 
                actualizarLuces("yellow", "VERIFICANDO SALIDA");
            } 
            else if (data.estado === "OK" && data.total > totalAnterior) {
                shoe.style.left = "830px";
                actualizarLuces("green", "APROBADO - SELLANDO");
                sealer.classList.add("sealing");
                
                totalAnterior = data.total;
                setTimeout(() => {
                    sealer.classList.remove("sealing");
                    shoe.style.left = "-60px"; 
                    imprimirTicketVirtual(data);
                }, 1200);
            } 
            else if (data.estado === "ERROR" && data.total > totalAnterior) {
                shoe.style.left = "630px"; 
                actualizarLuces("red", "ERROR: SELLADO BLOQUEADO");
                imprimirTicketVirtual(data);
                
                totalAnterior = data.total;
                setTimeout(() => {
                    shoe.style.left = "-60px"; 
                }, 3500);
            } 
            else if (data.entradaLive <= 5 && data.salidaLive <= 5) {
                if(!sealer.classList.contains("sealing") && data.estado === "ESPERANDO") {
                    shoe.style.left = "-60px";
                    actualizarLuces("green", "OPERACIÓN NORMAL");
                }
            }
        };

        function actualizarLuces(color, texto) {
            document.getElementById("lightRed").classList.remove("active");
            document.getElementById("lightYellow").classList.remove("active");
            document.getElementById("lightGreen").classList.remove("active");
            document.getElementById("estadoText").className = "";

            if (color === "green") {
                document.getElementById("lightGreen").classList.add("active");
                document.getElementById("estadoText").style.color = "#22c55e";
            } else if (color === "yellow") {
                document.getElementById("lightYellow").classList.add("active");
                document.getElementById("estadoText").style.color = "#f59e0b";
            } else if (color === "red") {
                document.getElementById("lightRed").classList.add("active");
                document.getElementById("estadoText").style.color = "#ef4444";
            }
            document.getElementById("estadoText").textContent = texto;
        }

        function imprimirTicketVirtual(data) {
            document.getElementById("tk-p1").textContent = "PESO ENTRADA: " + data.entrada.toFixed(1) + "g";
            document.getElementById("tk-p2").textContent = "PESO SALIDA: " + data.salida.toFixed(1) + "g";
            document.getElementById("tk-dif").textContent = "DIFERENCIA: " + data.diferencia.toFixed(1) + "g";
            document.getElementById("tk-status").textContent = "ESTADO: " + data.estado;
            
            let tk = document.getElementById("ticket");
            tk.style.display = "block";
            tk.style.opacity = 0.5;
            setTimeout(() => tk.style.opacity = 1, 300);
        }

        connection.onclose = function() { setTimeout(() => location.reload(), 3000); };
    </script>
</body>
</html>
)rawliteral";

// ================= PETICIONES HTTP / WEBSOCKETS =================

void paginaPrincipal() {
  server.send_P(200, "text/html", paginaHTML);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Manejador de eventos vacío (no necesitamos recibir datos desde el cliente web)
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

  // Inicialización de Balanzas
  balanza1.begin(DOUT1, SCK1);
  balanza1.set_scale(factor1);
  balanza1.tare();

  balanza2.begin(DOUT2, SCK2);
  balanza2.set_scale(factor2);
  balanza2.tare();

  // Conexión WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while(WiFi.status() != WL_CONNECTED){
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP del Servidor: ");
  Serial.println(WiFi.localIP());

  // Rutas HTTP e inicio de servidores
  server.on("/", paginaPrincipal);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ================= LOOP PRINCIPAL (PROCESAMIENTO ASÍNCRONO) =================

void loop() {
  server.handleClient();
  webSocket.loop();

  // Lectura síncrona NO BLOQUEANTE (Solo interviene si el integrado ya tiene datos listos)
  if (balanza1.is_ready()) {
    pesoActualEntrada = (balanza1.read() - balanza1.get_offset()) / factor1;
  }
  if (balanza2.is_ready()) {
    pesoActualSalida = (balanza2.read() - balanza2.get_offset()) / factor2;
  }

  // Máquina de estados de muestreo y validación (Cada 20 milisegundos)
  if (millis() - ultimoMuestreo >= 20) {
    ultimoMuestreo = millis();

    // ===== LÓGICA DE CONTROL: ZONA ENTRADA =====
    if (!piezaEntrada) {
      if (pesoActualEntrada > UMBRAL_DETECCION) {
        piezaEntrada = true;
        pesoMaxEntrada = pesoActualEntrada;
        estado = "ESPERANDO"; // Al entrar un elemento nuevo se reinicia el estado de aviso
      }
    } else {
      if (pesoActualEntrada > pesoMaxEntrada) {
        pesoMaxEntrada = pesoActualEntrada;
      }
      if (pesoActualEntrada < UMBRAL_DETECCION) {
        pesoEntrada = pesoMaxEntrada;
        piezaEntrada = false;
      }
    }

    // ===== LÓGICA DE CONTROL: ZONA SALIDA =====
    if (!piezaSalida) {
      if (pesoActualSalida > UMBRAL_DETECCION) {
        piezaSalida = true;
        pesoMaxSalida = pesoActualSalida;
      }
    } else {
      if (pesoActualSalida > pesoMaxSalida) {
        pesoMaxSalida = pesoActualSalida;
      }
      if (pesoActualSalida < UMBRAL_DETECCION) {
        pesoSalida = pesoMaxSalida;
        piezaSalida = false;

        // Evaluación diferencial de calidad física de empaque
        diferencia = abs(pesoEntrada - pesoSalida);
        totalPiezas++;

        if (diferencia <= TOLERANCIA) {
          estado = "OK";
          piezasOK++;
        } else {
          estado = "ERROR";
          piezasError++;
        }
      }
    }
  }

  // TRANSPORTE ULTRA RÁPIDO DE DATOS (Broadcast WebSockets cada 40 milisegundos)
  if (millis() - ultimoEnvioWS >= 40) {
    ultimoEnvioWS = millis();
    
    char json[256];
    snprintf(json, sizeof(json),
      "{\"entrada\":%.2f,\"salida\":%.2f,\"entradaLive\":%.2f,\"salidaLive\":%.2f,\"diferencia\":%.2f,\"estado\":\"%s\",\"total\":%d,\"ok\":%d,\"error\":%d}",
      pesoEntrada, pesoSalida, pesoActualEntrada, pesoActualSalida, diferencia, estado.c_str(), totalPiezas, piezasOK, piezasError);
    
    webSocket.broadcastTXT(json); // Envía los datos empaquetados instantáneamente a la web
  }
}
