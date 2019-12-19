#include <SPI.h>

 
const uint8_t PIN_SS = 7; // spi select pin

void setup() {
  Serial.begin(9600);

  // DWM1000 is active low by default
  pinMode(PIN_SS, OUTPUT);
  digitalWrite(PIN_SS, HIGH);
  SPI.begin();

  delay(100);
}

void loop() {
  
  delayMicroseconds(10);
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SS, LOW);

  SPI.transfer(0x00);
  for (int i = 0; i < 4; i++)  {
    Serial.println(SPI.transfer(0x00), HEX);
  }
  digitalWrite(PIN_SS, HIGH); 
    SPI.endTransaction();
    
    while (1) {}
}
