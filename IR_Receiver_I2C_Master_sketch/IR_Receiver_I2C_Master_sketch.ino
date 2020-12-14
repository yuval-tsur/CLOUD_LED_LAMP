#include <Wire.h>
#include <IRLib.h>
#define DEBUG // Uncomment to pass DEBUGging info via the serial port

#define IR_SENS_PIN 21
#define I2C_SLAVE_ADDRESS 5

IRrecv My_Receiver(IR_SENS_PIN);
IRdecode My_Decoder;
#define LED_indicator_pin 5 // Pin for LED to indicate IR signal detection
uint32_t Last_Signal = 0;

void setup() {
  #if defined(DEBUG)
	Serial.begin(9600);
  #endif
  pinMode(LED_indicator_pin,OUTPUT);

  // Join I2C bus as master
  Wire.begin();

  My_Receiver.enableIRIn();
}

void loop() {
  if (My_Receiver.GetResults(&My_Decoder)) {
      My_Decoder.decode();

	digitalWrite(LED_indicator_pin,HIGH);
    uint32_t IR_Signal = (My_Decoder.value);
	if (IR_Signal == 0xFFFFFFFF) {
      IR_Signal = Last_Signal;
	  delay(100); // Reduce repeated press rate
    } else {
      Last_Signal = IR_Signal;
      delay(400); // add extra delay when a new button is pressed to prevent from detecting two press if button is pressed too long
    }
    
	#if defined(DEBUG)
		Serial.println(IR_Signal,HEX);
	#endif
    // Send 4 bytes IR_Signal to slave
	byte irArray[4];
    irArray[0] = (byte)IR_Signal;
    irArray[1] = (byte)(IR_Signal >> 8);
    irArray[2] = (byte)(IR_Signal >> 16);
	irArray[3] = (byte)(IR_Signal >> 24);
        
	Wire.beginTransmission(I2C_SLAVE_ADDRESS);
	Wire.write(irArray,4);
	Wire.endTransmission();
	
    My_Receiver.resume();
    delay(100);
	digitalWrite(LED_indicator_pin,LOW);
  }
}