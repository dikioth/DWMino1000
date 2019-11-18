
#include <SPI.h>
#include <DW1000Ng.hpp>


const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = 7; // spi select pin

// DEBUG packet sent status and count
volatile unsigned long delaySent = 0;
String message;

device_configuration_t DEFAULT_CONFIG = {
    false,
    true,
    true,
    true,
    false,
    SFDMode::STANDARD_SFD,
    Channel::CHANNEL_5,
    DataRate::RATE_850KBPS,
    PulseFrequency::FREQ_16MHZ,
    PreambleLength::LEN_256,
    PreambleCode::CODE_3
};

void setup() {
  Serial.begin(9600);
  // initialize the driver
  DW1000Ng::initializeNoInterrupt(PIN_SS);
  DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
	//DW1000Ng::applyInterruptConfiguration(DEFAULT_INTERRUPT_CONFIG);

  DW1000Ng::setDeviceAddress(5); // sender device
  DW1000Ng::setDeviceAddress(6); // Reciever device
  DW1000Ng::setNetworkId(10);

  DW1000Ng::setAntennaDelay(16436);


  // attach callback for (successfully) sent messages
  //DW1000Ng::attachSentHandler(handleSent);
  // start a transmission
}

/*
void handleSent() {
  // status change on sent success
  sentAck = true;
}
*/

void loop() {
    if (Serial.available()){
          transmit();
    }
  
    // RECIEVE 
    recieve();
}



void transmit() {
  // transmit some data
  String msg = Serial.readString();
  DW1000Ng::setTransmitData(msg);
  // delay sending the message for the given amount
  delay(1000);
  DW1000Ng::startTransmit(TransmitMode::IMMEDIATE);
  delaySent = millis();
  while(!DW1000Ng::isTransmitDone()) {
    #if defined(ESP8266)
    yield();
    #endif
  }
  DW1000Ng::clearTransmitStatus();
}



void recieve(){
  DW1000Ng::startReceive();
  while(!DW1000Ng::isReceiveDone()) {
    #if defined(ESP8266)
    yield();
    #endif
  }
  DW1000Ng::clearReceiveStatus();
  // get data as string
  DW1000Ng::getReceivedData(message);
  Serial.println(message);
}