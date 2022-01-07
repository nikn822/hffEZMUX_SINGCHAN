#include <i2c_t3.h>
#include <hffEZMUX_SINGCHAN.h>

#define TCA 0x70
#define TCA2 0x77

TSL2591 test = TSL2591();
void setup() {
  Serial.begin(9600);
  Serial.println("TESTING");
}
   
void loop() {
  test.simpleRead(2,3);
  Serial.println("TSL Sensor #1");
  Serial.print(F("[ ")); Serial.print(test.sro.ms); Serial.print(F(" ms ] "));
  Serial.print(F("Luminosity: "));
  Serial.println(test.sro.lum, DEC);

  test.simpleRead(2,2);
  Serial.println("TSL Sensor #2");
  Serial.print(F("[ ")); Serial.print(test.sro.ms); Serial.print(F(" ms ] "));
  Serial.print(F("Luminosity: "));
  Serial.println(test.sro.lum, DEC);

  test.simpleRead(4,3);
  Serial.println("TSL Sensor #3");
  Serial.print(F("[ ")); Serial.print(test.sro.ms); Serial.print(F(" ms ] "));
  Serial.print(F("Luminosity: "));
  Serial.println(test.sro.lum, DEC);

  test.simpleRead(4,2);
  Serial.println("TSL Sensor #4");
  Serial.print(F("[ ")); Serial.print(test.sro.ms); Serial.print(F(" ms ] "));
  Serial.print(F("Luminosity: "));
  Serial.println(test.sro.lum, DEC);
  Serial.println();

  delay(500);

}