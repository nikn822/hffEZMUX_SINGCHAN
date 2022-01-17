#include <i2c_t3.h>
#include <hffEZMUX_SINGCHAN.h>

#define TCA 0x70
#define TCA2 0x77

Nanolab test = Nanolab();
void setup() {
  //Serial.begin(9600);
  //Serial.println("TESTING");
}
   
void loop() {

  test.saveSD();
  delay(50000);

}
