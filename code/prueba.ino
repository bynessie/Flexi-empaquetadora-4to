#include "HX711.h"

#define DOUT 4
#define CLK 5

HX711 scale;

float calibration_factor = 2280.0;

void setup() {

  Serial.begin(115200);

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor);
  scale.tare();

  Serial.println("Bascula lista");
}

void loop() {

  Serial.print("Peso: ");
  Serial.print(scale.get_units(5));
  Serial.println(" g");

  delay(500);
}
