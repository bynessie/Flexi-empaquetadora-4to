#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h> // REQUISITO: Instalar "WebSockets" desde el gestor de librerías
#include "HX711.h"

// ================= CONFIGURACIÓN WIFI =================
const char* ssid = "ZAM_ASUS";
const char* password = "12345678";

// ================= CREDENCIALES DE ACCESO =================
const char* usuarioValido = "Flexillinis";
const char* passwordValido = "12345678";

// ================= CONFIGURACIÓN HX711 =================
#define DOUT1 16
#define SCK1  4

#define DOUT2 17
#define SCK2  5

HX711 balanza1;
HX711 balanza2;

float factor1 = 214.3;
float factor2 = 225.0;

// ================= PARÁMETROS DE VALIDACIÓN =================
const float UMBRAL_DETECCION = 5.0;  // Peso mínimo para detectar un calzado (en gramos)
const float TOLERANCIA = 10.0;       // Tolerancia máxima permitida entre Entrada y Salida

// ================= VARIABLES DE CONTROL =================
bool programaIniciado = false; // Control de ejecución del programa principal

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
WebSocketsServer webSocket = WebSocketsServer(81);

// ================= INTERFAZ WEB: LOGIN =================
const char paginaLoginHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login - Sistema Flexi</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', sans-serif; }
        body { background-color: #f8fafc; display: flex; align-items: center; justify-content: center; min-height: 100vh; }
        .login-card { background: #ffffff; border-radius: 15px; padding: 40px; width: 100%; max-width: 400px; box-shadow: 0 10px 25px rgba(0,0,0,0.05); border-top: 5px solid #dc2626; text-align: center; }
        h2 { color: #0f172a; margin-bottom: 10px; font-size: 1.8rem; }
        p { color: #64748b; font-size: 0.9rem; margin-bottom: 25px; }
        .input-group { text-align: left; margin-bottom: 20px; }
        label { display: block; font-size: 0.85rem; font-weight: 600; color: #475569; margin-bottom: 6px; text-transform: uppercase; }
        input { width: 100%; padding: 12px; border: 2px solid #e2e8f0; border-radius: 8px; font-size: 1rem; transition: border 0.2s; }
        input:focus { border-color: #dc2626; outline: none; }
        .btn-submit { width: 100%; background: #dc2626; color: white; border: none; padding: 14px; border-radius: 8px; font-size: 1rem; font-weight: bold; cursor: pointer; transition: background 0.2s; margin-top: 10px; }
        .btn-submit:hover { background: #b91c1c; }
        .error-msg { color: #dc2626; font-size: 0.85rem; margin-top: 15px; display: none; font-weight: 600; }
    </style>
</head>
<body>
    <div class="login-card">
        <h2>Control de Acceso</h2>
        <p>Inicie sesión para gestionar el sistema de empaquetado</p>
        <form action="/login" method="POST">
            <div class="input-group">
                <label>Usuario</label>
                <input type="text" name="usuario" required autocomplete="off">
            </div>
            <div class="input-group">
                <label>Contraseña</label>
                <input type="password" name="password" required>
            </div>
            <button type="submit" class="btn-submit">Ingresar al Sistema</button>
        </form>
        <div class="error-msg" id="errorDiv">Credenciales incorrectas. Intente de nuevo.</div>
    </div>
    <script>
        if (window.location.search.includes("error=1")) {
            document.getElementById("errorDiv").style.display = "block";
        }
    </script>
</body>
</html>
)rawliteral";

// ================= INTERFAZ WEB: DASHBOARD PRINCIPAL =================
const char paginaHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Flexi Sistema Automático de Recolección y Empaquetado</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', sans-serif; }
        body { background-color: #f8fafc; color: #1e293b; padding: 20px; min-height: 100vh; }
        
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; border-bottom: 3px solid #dc2626; padding-bottom: 15px; }
        h1 { font-size: 2.2rem; color: #0f172a; }
        .sub { color: #64748b; font-size: 1rem; margin-top: 5px; font-weight: 500; }

        .header-controls { display: flex; gap: 12px; }
        .btn-control { padding: 12px 24px; border-radius: 8px; font-weight: bold; border: none; cursor: pointer; font-size: 0.95rem; transition: all 0.2s ease; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
        .btn-start { background-color: #16a34a; color: white; }
        .btn-start:hover { background-color: #15803d; }
        .btn-stop { background-color: #dc2626; color: white; }
        .btn-stop:hover { background-color: #b91c1c; }
        .btn-logout { background-color: #64748b; color: white; text-decoration: none; display: flex; align-items: center; padding: 12px 18px; border-radius: 8px; font-weight: bold; font-size: 0.9rem; }
        .btn-logout:hover { background-color: #475569; }

        .dashboard { display: grid; grid-template-columns: 3fr 1fr; gap: 20px; max-width: 1300px; margin: auto; align-items: start; }
        .left-panel { display: flex; flex-direction: column; gap: 20px; }
        .right-panel { position: sticky; top: 20px; }
        
        .conveyor-container { background: #ffffff; border-radius: 20px; padding: 35px 110px 35px 35px; border: 2px solid #e2e8f0; position: relative; box-shadow: 0 4px 12px rgba(0,0,0,0.05); overflow: visible; }
        .conveyor-belt { height: 80px; background: #e2e8f0; border-radius: 40px; position: relative; display: flex; align-items: center; overflow: hidden; border: 4px solid #cbd5e1; z-index: 1; }
        
        .divider { position: absolute; top: 0; width: 4px; height: 100%; background: #94a3b8; opacity: 0.5; }
        .div-1 { left: 25%; } .div-2 { left: 55%; } .div-3 { left: 78%; }

        .zone-label { position: absolute; font-size: 0.75rem; font-weight: bold; text-transform: uppercase; letter-spacing: 1px; color: #dc2626; top: 8px; z-index: 2; }
        .z-in { left: 45px; } .z-out { left: 525px; } .z-seal { right: 35px; }

        .shoe-box { width: 65px; height: 45px; background: #ffffff; border-radius: 6px; position: absolute; display: flex; align-items: center; justify-content: center; font-size: 0.75rem; font-weight: bold; text-transform: uppercase; color: #dc2626; transition: left 0.4s ease-out; border: 2px solid #dc2626; box-shadow: 0 4px 8px rgba(220, 38, 38, 0.15); z-index: 3; }
        
        .sealer-machine { position: absolute; right: 25px; top: 20px; width: 75px; height: 110px; background: #f1f5f9; border-radius: 10px; border: 2px solid #cbd5e1; display: flex; flex-direction: column; align-items: center; justify-content: center; font-size: 0.8rem; text-align: center; font-weight: bold; color: #475569; z-index: 10; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
        .sealer-piston { width: 100%; height: 15px; background: #dc2626; position: absolute; top: 0; transition: transform 0.2s; }
        .sealing .sealer-piston { transform: translateY(35px); background: #b91c1c; }

        .grid-stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .card { background: #ffffff; border-radius: 15px; padding: 15px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); border: 1px solid #e2e8f0; }
        .label { font-size: 0.85rem; color: #64748b; text-transform: uppercase; font-weight: 600; }
        .value { font-size: 1.8rem; font-weight: bold; margin-top: 5px; color: #0f172a; }
        
        .rojo-detalle { color: #dc2626; }
        .gris-obscuro { color: #334155; }
        .verde-ok { color: #16a34a; }

        .history-card { background: #ffffff; border-radius: 15px; padding: 20px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); border: 1px solid #e2e8f0; margin-top: 20px; }
        .history-table { width: 100%; border-collapse: collapse; margin-top: 15px; font-size: 0.9rem; text-align: left; }
        .history-table th { background: #f1f5f9; color: #475569; padding: 10px; font-weight: 600; border-bottom: 2px solid #cbd5e1; }
        .history-table td { padding: 10px; border-bottom: 1px solid #e2e8f0; color: #334155; vertical-align: middle; }
        .status-badge { font-weight: bold; padding: 3px 8px; border-radius: 5px; text-transform: uppercase; font-size: 0.8rem; display: inline-block; }
        .badge-ok { background: #dcfce7; color: #16a34a; }
        .badge-error { background: #fee2e2; color: #dc2626; }

        .btn-recibo { background-color: #334155; color: white; border: none; padding: 5px 10px; border-radius: 5px; cursor: pointer; font-size: 0.8rem; font-weight: 600; transition: background 0.2s; }
        .btn-recibo:hover { background-color: #dc2626; }

        .tower-card { display: flex; flex-direction: column; align-items: center; justify-content: center; background: #ffffff; border-radius: 15px; padding: 25px; border: 1px solid #e2e8f0; box-shadow: 0 4px 12px rgba(0,0,0,0.05); height: 380px; width: 100%; max-height: 380px; }
        .light-tower { width: 90px; background: #f1f5f9; padding: 18px; border-radius: 15px; display: flex; flex-direction: column; gap: 15px; border: 4px solid #cbd5e1; }
        .light { width: 54px; height: 54px; border-radius: 50%; background: #cbd5e1; opacity: 0.2; transition: all 0.2s; box-shadow: inset 0 0 10px rgba(0,0,0,0.1); }
        
        .light.active.green { background: #16a34a; opacity: 1; box-shadow: 0 0 25px #16a34a, inset 0 0 10px #ffffff80; }
        .light.active.yellow { background: #ca8a04; opacity: 1; box-shadow: 0 0 25px #ca8a04, inset 0 0 10px #ffffff80; }
        .light.active.red { background: #dc2626; opacity: 1; box-shadow: 0 0 35px #dc2626, inset 0 0 10px #ffffff80; }
        
        #estadoText { margin-top: 20px; font-weight: bold; font-size: 1.2rem; text-align: center; color: #0f172a; min-height: 40px; text-transform: uppercase; letter-spacing: 0.5px; }
        
        .ticket-box { background: #ffffff; color: #1e293b; padding: 15px; border-radius: 10px; font-family: 'Courier New', monospace; font-size: 0.8rem; margin-top: 15px; border-left: 5px solid #dc2626; display: none; box-shadow: 0 4px 12px rgba(0,0,0,0.05); border-top: 1px solid #e2e8f0; border-right: 1px solid #e2e8f0; border-bottom: 1px solid #e2e8f0; }

        .modal { display: none; position: fixed; z-index: 100; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.5); align-items: center; justify-content: center; }
        .modal-content { background-color: white; padding: 20px; border-radius: 10px; width: 350px; box-shadow: 0 4px 20px rgba(0,0,0,0.2); text-align: center; }
        
        .etiqueta-print { border: 3px double #000; padding: 15px; background: white; font-family: 'Courier New', monospace; text-align: left; color: black; margin-bottom: 15px; box-shadow: inset 0 0 5px rgba(0,0,0,0.1); }
        .etiqueta-print h2 { text-align: center; font-size: 1.3rem; border-bottom: 2px dashed #000; padding-bottom: 5px; margin-bottom: 10px; color: black; }
        .etiqueta-print .item { margin: 6px 0; font-size: 0.9rem; }
        
        .barcode-container { margin-top: 20px; display: flex; flex-direction: column; align-items: center; border-top: 1px dashed #000; padding-top: 12px; }
        .real-barcode { width: 220px; height: 50px; background: linear-gradient(90deg, #000 0%, #000 2%, #fff 2%, #fff 4%, #000 4%, #000 5%, #fff 5%, #fff 8%, #000 8%, #000 12%, #fff 12%, #fff 13%, #000 13%, #000 14%, #fff 14%, #fff 18%, #000 18%, #000 19%, #fff 19%, #fff 22%, #000 22%, #000 26%, #fff 26%, #fff 28%, #000 28%, #000 29%, #fff 29%, #fff 32%, #000 32%, #000 33%, #fff 33%, #fff 37%, #000 37%, #000 41%, #fff 41%, #fff 43%, #000 43%, #000 44%, #fff 44%, #fff 48%, #000 48%, #000 52%, #fff 52%, #fff 55%, #000 55%, #000 57%, #fff 57%, #fff 58%, #000 58%, #000 62%, #fff 62%, #fff 64%, #000 64%, #000 65%, #fff 65%, #fff 69%, #000 69%, #000 70%, #fff 70%, #fff 74%, #000 74%, #000 78%, #fff 78%, #fff 80%, #000 80%, #000 81%, #fff 81%, #fff 85%, #000 85%, #000 89%, #fff 89%, #fff 91%, #000 91%, #000 95%, #fff 95%, #fff 97%, #000 97%, #000 100%); }
        .barcode-text { font-family: 'Arial', sans-serif; font-size: 0.75rem; letter-spacing: 4px; margin-top: 4px; text-align: center; font-weight: bold; }
        
        .modal-actions { display: flex; gap: 10px; justify-content: center; }
        .btn-print { background-color: #16a34a; color: white; border: none; padding: 8px 15px; border-radius: 5px; cursor: pointer; font-weight: bold; }
        .btn-close { background-color: #dc2626; color: white; border: none; padding: 8px 15px; border-radius: 5px; cursor: pointer; font-weight: bold; }

        @media print {
            body * { visibility: hidden; }
            #areaImpresion, #areaImpresion * { visibility: visible; }
            #areaImpresion { position: absolute; left: 0; top: 0; width: 100%; border: none; }
            .modal, .modal-content { background: none; box-shadow: none; display: block !important; position: absolute; left:0; top:0; }
            .modal-actions, .btn-close { display: none !important; }
        }
    </style>
</head>
<body>

    <header>
        <div>
            <h1>Sistema Automático de Recolección y Empaquetado</h1>
            <div class="sub">Monitoreo del Conveyor, Control de Calidad y Sellado</div>
        </div>
        <div class="header-controls">
            <button id="btnPrograma" class="btn-control btn-start" onclick="togglePrograma()">INICIAR PROGRAMA</button>
            <a href="/logout" class="btn-logout">Cerrar Sesión</a>
        </div>
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
                    <div class="shoe-box" id="shoeBox" style="left: -70px;">LOTE-1</div>
                </div>

                <div class="sealer-machine" id="sealer">
                    <div class="sealer-piston"></div>
                    <span style="margin-top:20px; z-index:1;">SELLADO</span>
                </div>
            </div>

            <div class="grid-stats">
                <div class="card"><div class="label">Peso Entrada</div><div id="entrada" class="value gris-obscuro">0.0 g</div></div>
                <div class="card"><div class="label">Peso Salida</div><div id="salida" class="value gris-obscuro">0.0 g</div></div>
                <div class="card"><div class="label">Diferencia</div><div id="diferencia" class="value rojo-detalle">0.0 g</div></div>
                <div class="card"><div class="label">Lote en Curso</div><div id="loteActual" class="value rojo-detalle">LOTE-1</div></div>
                <div class="card"><div class="label">Unidades Procesadas</div><div id="total" class="value gris-obscuro">0</div></div>
                <div class="card"><div class="label">Empaques OK</div><div id="ok" class="value verde-ok">0</div></div>
                <div class="card"><div class="label">Empaques Bloqueados</div><div id="error" class="value rojo-detalle">0</div></div>
            </div>

            <div class="ticket-box" id="ticket">
                <div style="font-weight:bold; color:#dc2626;">====== REGISTRO DE EMPAQUE ======</div>
                <div id="tk-id">LOTE: LOTE-0</div>
                <div id="tk-p1">PESO ENTRADA: 0.0g</div>
                <div id="tk-p2">PESO SALIDA: 0.0g</div>
                <div id="tk-dif">DIFERENCIA: 0.0g</div>
                <div id="tk-status" style="font-weight:bold;">ESTADO: -</div>
                <div style="color:#dc2626;">==================================</div>
            </div>

            <div class="history-card">
                <div class="label" style="color: #dc2626; font-size: 1rem;">Historial de Lotes Procesados</div>
                <table class="history-table">
                    <thead>
                        <tr>
                            <th>Identificador</th>
                            <th>Peso Entrada</th>
                            <th>Peso Salida</th>
                            <th>Diferencia</th>
                            <th>Dictamen Final</th>
                            <th>Acciones</th>
                        </tr>
                    </thead>
                    <tbody id="historyBody">
                    </tbody>
                </table>
            </div>
        </div>

        <div class="right-panel">
            <div class="tower-card">
                <div class="label" style="margin-bottom: 15px; color:#dc2626; font-size:0.9rem;">Torre de Luces</div>
                <div class="light-tower">
                    <div class="light red" id="lightRed"></div>
                    <div class="light yellow" id="lightYellow"></div>
                    <div class="light green" id="lightGreen"></div>
                </div>
                <div id="estadoText">ESPERANDO</div>
            </div>
        </div>
    </div>

    <div id="modalTicket" class="modal">
        <div class="modal-content">
            <div class="etiqueta-print" id="areaImpresion">
                <h2>CONTROL DE CALIDAD</h2>
                <div class="item"><strong>SISTEMA:</strong> Conveyor Flexi</div>
                <div class="item"><strong>ID LOTE:</strong> <span id="lblLote">-</span></div>
                <div class="item"><strong>FECHA:</strong> <span id="lblFecha">-</span></div>
                <div class="item">----------------------------</div>
                <div class="item"><strong>P. ENTRADA:</strong> <span id="lblEntrada">0.0 g</span></div>
                <div class="item"><strong>P. SALIDA:</strong> <span id="lblSalida">0.0 g</span></div>
                <div class="item"><strong>DIFERENCIA:</strong> <span id="lblDif">0.0 g</span></div>
                <div class="item">----------------------------</div>
                <div class="item"><strong>DICTAMEN:</strong> <span id="lblDictamen">-</span></div>
                
                <div class="barcode-container">
                    <div class="real-barcode"></div>
                    <div class="barcode-text" id="lblBarcodeNum">000000000000</div>
                </div>
            </div>
            <div class="modal-actions">
                <button class="btn-print" onclick="ejecutarImpresion()">Imprimir Etiqueta</button>
                <button class="btn-close" onclick="cerrarModal()">Cerrar</button>
            </div>
        </div>
    </div>

    <script>
        let connection = new WebSocket('ws://' + location.hostname + ':81/');
        let totalAnterior = 0;
        let estadoAnimacion = "INICIO"; 
        let ultimoEstadoSemaforo = "green"; 
        let ultimoTextoSemaforo = "OPERACIÓN NORMAL";
        let localProgramaIniciado = false;

        connection.onmessage = function(e) {
            let data = JSON.parse(e.data);
            
            localProgramaIniciado = data.progIniciado;
            let btn = document.getElementById("btnPrograma");
            if(localProgramaIniciado) {
                btn.textContent = "DETENER PROGRAMA";
                btn.className = "btn-control btn-stop";
            } else {
                btn.textContent = "INICIAR PROGRAMA";
                btn.className = "btn-control btn-start";
            }

            document.getElementById("entrada").textContent = data.entradaLive.toFixed(1) + " g";
            document.getElementById("salida").textContent = data.salidaLive.toFixed(1) + " g";
            document.getElementById("diferencia").textContent = data.diferencia.toFixed(1) + " g";
            document.getElementById("total").textContent = data.total;
            document.getElementById("ok").textContent = data.ok;
            document.getElementById("error").textContent = data.error;

            let nombreLote = "LOTE-" + (data.total + 1);
            document.getElementById("loteActual").textContent = nombreLote;

            let shoe = document.getElementById("shoeBox");
            let sealer = document.getElementById("sealer");
            
            if(!localProgramaIniciado) {
                actualizarLuces("yellow", "SISTEMA EN PAUSA");
                return;
            }

            if (data.entradaLive > 5 && estadoAnimacion !== "PROCESANDO") {
                estadoAnimacion = "ENTRADA";
                shoe.style.left = "40px"; 
                shoe.textContent = "LOTE-" + (data.total + 1);
                actualizarLuces("yellow", "VERIFICANDO ENTRADA");
            } 
            else if (data.salidaLive > 5 && estadoAnimacion !== "PROCESANDO") {
                estadoAnimacion = "SALIDA";
                shoe.style.left = "520px"; 
                shoe.textContent = "LOTE-" + (data.total + 1);
                actualizarLuces("yellow", "VERIFICANDO SALIDA");
            } 
            else if (data.estado === "OK" && data.total > totalAnterior) {
                estadoAnimacion = "PROCESANDO";
                shoe.style.left = "640px";
                
                ultimoEstadoSemaforo = "green";
                ultimoTextoSemaforo = "LOTE ANTERIOR: APROBADO";
                actualizarLuces("green", "APROBADO - SELLANDO");
                sealer.classList.add("sealing");
                
                let loteFinalizado = "LOTE-" + data.total;
                totalAnterior = data.total;
                
                setTimeout(() => {
                    sealer.classList.remove("sealing");
                    shoe.style.left = "-70px"; 
                    imprimirTicketVirtual(data, loteFinalizado);
                    agregarAlHistorialHTML(loteFinalizado, data.entrada, data.salida, data.diferencia, "OK");
                    estadoAnimacion = "INICIO";
                    actualizarLuces(ultimoEstadoSemaforo, ultimoTextoSemaforo);
                }, 1200);
            } 
            else if (data.estado === "ERROR" && data.total > totalAnterior) {
                estadoAnimacion = "PROCESANDO";
                shoe.style.left = "520px"; 
                
                ultimoEstadoSemaforo = "red";
                ultimoTextoSemaforo = "LOTE ANTERIOR: RECHAZADO";
                actualizarLuces("red", "ERROR: BLOQUEADO");
                
                let loteFinalizado = "LOTE-" + data.total;
                totalAnterior = data.total;
                
                imprimirTicketVirtual(data, loteFinalizado);
                agregarAlHistorialHTML(loteFinalizado, data.entrada, data.salida, data.diferencia, "ERROR");
                
                setTimeout(() => {
                    shoe.style.left = "-70px"; 
                    estadoAnimacion = "INICIO";
                    actualizarLuces(ultimoEstadoSemaforo, ultimoTextoSemaforo);
                }, 3500);
            } 
            else if (data.entradaLive <= 5 && data.salidaLive <= 5 && estadoAnimacion === "INICIO") {
                if(!sealer.classList.contains("sealing")) {
                    shoe.style.left = "-70px";
                    shoe.textContent = "LOTE-" + (data.total + 1);
                    actualizarLuces(ultimoEstadoSemaforo, ultimoTextoSemaforo);
                }
            }
        };

        function togglePrograma() {
            localProgramaIniciado = !localProgramaIniciado;
            connection.send(localProgramaIniciado ? "START" : "STOP");
        }

        function actualizarLuces(color, texto) {
            document.getElementById("lightRed").classList.remove("active");
            document.getElementById("lightYellow").classList.remove("active");
            document.getElementById("lightGreen").classList.remove("active");

            if (color === "green") {
                document.getElementById("lightGreen").classList.add("active");
                document.getElementById("estadoText").style.color = "#16a34a";
            } else if (color === "yellow") {
                document.getElementById("lightYellow").classList.add("active");
                document.getElementById("estadoText").style.color = "#ca8a04";
            } else if (color === "red") {
                document.getElementById("lightRed").classList.add("active");
                document.getElementById("estadoText").style.color = "#dc2626";
            }
            document.getElementById("estadoText").textContent = texto;
        }

        function imprimirTicketVirtual(data, loteId) {
            document.getElementById("tk-id").textContent = "LOTE: " + loteId;
            document.getElementById("tk-p1").textContent = "PESO ENTRADA: " + data.entrada.toFixed(1) + "g";
            document.getElementById("tk-p2").textContent = "PESO SALIDA: " + data.salida.toFixed(1) + "g";
            document.getElementById("tk-dif").textContent = "DIFERENCIA: " + data.diferencia.toFixed(1) + "g";
            document.getElementById("tk-status").textContent = "ESTADO: " + (data.estado === "OK" ? "APROBADO" : "RECHAZADO");
            
            if(data.estado === "OK") {
                document.getElementById("tk-status").style.color = "#16a34a";
            } else {
                document.getElementById("tk-status").style.color = "#dc2626";
            }
            
            let tk = document.getElementById("ticket");
            tk.style.display = "block";
            tk.style.opacity = 0.5;
            setTimeout(() => tk.style.opacity = 1, 300);
        }

        function agregarAlHistorialHTML(lote, entrada, salida, dif, resultado) {
            let tbody = document.getElementById("historyBody");
            let row = document.createElement("tr");
            let badgeClass = resultado === "OK" ? "badge-ok" : "badge-error";
            let badgeText = resultado === "OK" ? "Aprobado" : "Rechazado";

            row.innerHTML = `
                <td><strong>${lote}</strong></td>
                <td>${entrada.toFixed(1)} g</td>
                <td>${salida.toFixed(1)} g</td>
                <td>${dif.toFixed(1)} g</td>
                <td><span class="status-badge ${badgeClass}">${badgeText}</span></td>
                <td><button class="btn-recibo" onclick="abrirRecibo('${lote}', ${entrada}, ${salida}, ${dif}, '${badgeText}')">Ver Recibo</button></td>
            `;
            tbody.insertBefore(row, tbody.firstChild);
        }

        function abrirRecibo(lote, entrada, salida, dif, dictamen) {
            document.getElementById("lblLote").textContent = lote;
            document.getElementById("lblEntrada").textContent = entrada.toFixed(1) + " g";
            document.getElementById("lblSalida").textContent = salida.toFixed(1) + " g";
            document.getElementById("lblDif").textContent = dif.toFixed(1) + " g";
            document.getElementById("lblDictamen").textContent = dictamen;
            
            let colorDictamen = (dictamen === "Aprobado") ? "#16a34a" : "#dc2626";
            document.getElementById("lblDictamen").style.color = colorDictamen;

            let ahora = new Date();
            let fechaStr = ahora.toLocaleDateString() + " " + ahora.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
            document.getElementById("lblFecha").textContent = fechaStr;

            let numeroLote = lote.replace("LOTE-", "");
            let codigoFormateado = "FLX-" + numeroLote.padStart(5, '0');
            document.getElementById("lblBarcodeNum").textContent = codigoFormateado;

            document.getElementById("modalTicket").style.display = "flex";
        }

        function cerrarModal() { document.getElementById("modalTicket").style.display = "none"; }
        function ejecutarImpresion() { window.print(); }

        window.onclick = function(event) {
            let modal = document.getElementById("modalTicket");
            if (event.target == modal) modal.style.display = "none";
        }

        connection.onclose = function() { setTimeout(() => location.reload(), 3000); };
    </script>
</body>
</html>
)rawliteral";

// ================= COMPROBACIÓN DE SESIÓN (COOKIES) =================

bool estaAutenticado() {
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      return true;
    }
  }
  return false;
}

// ================= CONTROLADORES HTTP =================

void paginaPrincipal() {
  if (!estaAutenticado()) {
    server.sendHeader("Location", "/login");
    server.send(303);
    return;
  }
  server.send_P(200, "text/html", paginaHTML);
}

void vistaLogin() {
  if (estaAutenticado()) {
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }
  server.send_P(200, "text/html", paginaLoginHTML);
}

void procesarLogin() {
  if (server.hasArg("usuario") && server.hasArg("password")) {
    if (server.arg("usuario") == usuarioValido && server.arg("password") == passwordValido) {
      server.sendHeader("Set-Cookie", "ESPSESSIONID=1; Path=/; HttpOnly");
      server.sendHeader("Location", "/");
      server.send(303);
      return;
    }
  }
  server.sendHeader("Location", "/login?error=1");
  server.send(303);
}

void procesarLogout() {
  // CAMBIO CLAVE: Al cerrar sesión se fuerza el paro automático del conveyor/lógica de pesado
  programaIniciado = false;
  estado = "ESPERANDO";
  Serial.println("Sesión cerrada. Programa pausado automáticamente por seguridad.");

  // Sobrescribe la cookie para borrarla y fuerza su expiración inmediata
  server.sendHeader("Set-Cookie", "ESPSESSIONID=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly");
  server.sendHeader("Location", "/login");
  server.send(303);
}

// ================= CONTROLADOR WEBSOCKET =================

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String comando = String((char*)payload);
    if (comando == "START") {
      programaIniciado = true;
      Serial.println("Programa INICIADO desde interfaz web.");
    } else if (comando == "STOP") {
      programaIniciado = false;
      Serial.println("Programa DETENIDO desde interfaz web.");
    }
  }
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

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while(WiFi.status() != WL_CONNECTED){
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP del Servidor: ");
  Serial.println(WiFi.localIP());

  // Configuración del servidor HTTP para rastrear Cookies
  const char* encabezados[] = {"Cookie"};
  server.collectHeaders(encabezados, 1);

  // ================= ENRUTAMIENTO HTTP =================
  server.on("/", paginaPrincipal);
  server.on("/login", HTTP_GET, vistaLogin);
  server.on("/login", HTTP_POST, procesarLogin);
  server.on("/logout", procesarLogout); 

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ================= LOOP PRINCIPAL =================

void loop() {
  server.handleClient();
  webSocket.loop();

  // 1. Obtención de lecturas continuas (Live Metrics)
  if (balanza1.is_ready()) {
    pesoActualEntrada = (balanza1.read() - balanza1.get_offset()) / factor1;
  }
  if (balanza2.is_ready()) {
    pesoActualSalida = (balanza2.read() - balanza2.get_offset()) / factor2;
  }

  // 2. Lógica de validación (Solo si el programa ha sido iniciado)
  if (programaIniciado) {
    if (millis() - ultimoMuestreo >= 20) {
      ultimoMuestreo = millis();

      // LÓGICA: ENTRADA
      if (!piezaEntrada) {
        if (pesoActualEntrada > UMBRAL_DETECCION) {
          piezaEntrada = true;
          pesoMaxEntrada = pesoActualEntrada;
          estado = "ESPERANDO"; 
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

      // LÓGICA: SALIDA
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
  } else {
    estado = "ESPERANDO";
  }

  // 3. Envío de datos por WebSockets (Incluye la clave "progIniciado")
  if (millis() - ultimoEnvioWS >= 40) {
    ultimoEnvioWS = millis();
    
    char json[256];
    snprintf(json, sizeof(json),
      "{\"entrada\":%.2f,\"salida\":%.2f,\"entradaLive\":%.2f,\"salidaLive\":%.2f,\"diferencia\":%.2f,\"estado\":\"%s\",\"total\":%d,\"ok\":%d,\"error\":%d,\"progIniciado\":%s}",
      pesoEntrada, pesoSalida, pesoActualEntrada, pesoActualSalida, diferencia, estado.c_str(), totalPiezas, piezasOK, piezasError, programaIniciado ? "true" : "false");
    
    webSocket.broadcastTXT(json); 
  }
}
