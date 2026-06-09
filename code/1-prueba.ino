#include "HX711.h"

// Pines del HX711
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN  = 4;

HX711 balanza;

void setup() {
  Serial.begin(9600);

  // Inicializar HX711
  balanza.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // Factor de calibración
  balanza.set_scale(1811.59);

  // Tara (pone el peso actual como cero)
  balanza.tare();

  Serial.println("Bascula lista");
}

void loop() {

  float peso = balanza.get_units(5);

  Serial.print("Peso: ");
  Serial.print(peso);   // quita el signo menos si aparece invertido
  Serial.println(" g");

  delay(500);
}
