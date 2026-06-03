#include "HX711.h"

const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN  = 4;

HX711 scale;

void setup() {
  Serial.begin(115200);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.tare();

  Serial.println("HX711 listo");
}

void loop() {

  Serial.print("Lectura: ");
  Serial.println(scale.get_units());

  delay(500);
}
