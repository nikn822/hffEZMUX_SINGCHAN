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

  for(int column = 0; column<4; column++){
    for(int row = 0; row <4; row ++){
      test.LED(column,row);
      delay(50);
    }
    
  }
  

}
