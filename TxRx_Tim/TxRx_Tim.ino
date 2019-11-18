
#include <SPI.h>
#include <DW1000.h>
#include <DW1000Ng.hpp>




const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = 7; // spi select pin

// DEBUG packet sent status and count
volatile unsigned long delaySent = 0;
int16_t sentNum = 0; // todo check int type
int16_t numReceived = 0; // todo check int type


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
  // DEBUG monitoring
  Serial.begin(9600);
  // initialize the driver
  DW1000Ng::initializeNoInterrupt(PIN_SS);
  Serial.println(F("DW1000Ng initialized ..."));

  DW1000Ng::applyConfiguration(DEFAULT_CONFIG);

  DW1000Ng::setDeviceAddress(5);
  DW1000Ng::setNetworkId(10);
  
  DW1000Ng::setAntennaDelay(16436);
  Serial.println(F("Committed configuration ..."));
  // DEBUG chip info and registers pretty printed
  char msg[128];
  DW1000Ng::getPrintableDeviceIdentifier(msg);
  Serial.print("Device ID: "); Serial.println(msg);
  DW1000Ng::getPrintableExtendedUniqueIdentifier(msg);
  Serial.print("Unique ID: "); Serial.println(msg);
  DW1000Ng::getPrintableNetworkIdAndShortAddress(msg);
  Serial.print("Network ID & Device Address: "); Serial.println(msg);
  DW1000Ng::getPrintableDeviceMode(msg);
  Serial.print("Device mode: "); Serial.println(msg);

  // attach callback for (successfully) sent messages
  //DW1000Ng::attachSentHandler(handleSent);
}

void loop() {
    char c;
    String msg;
    if (Serial.available())
    {
        
        c = Serial.read();
        Serial.println("char: " + c);
        msg = String("msg: " + msg + c);
        
        transmit(msg); // send msg or c as input
        // update and print some information about the sent message
        Serial.print("ARDUINO delay sent [ms] ... "); Serial.println(millis() - delaySent);
        uint64_t newSentTime = DW1000Ng::getTransmitTimestamp();
        Serial.print("Processed packet ... #"); Serial.println(sentNum);
    }
}

//void receive() {
//    DW1000Ng::startReceive();
//    while(!DW1000Ng::isReceiveDone()) {
//      #if defined(ESP8266)
//      yield();
//      #endif
//    }
//    DW1000Ng::clearReceiveStatus();
//    numReceived++;
//    // get data as string
//    DW1000Ng::getReceivedData(message);
//    Serial.print("Received message ... #"); Serial.println(numReceived);
//    Serial.print("Data is ... "); Serial.println(message);
//    Serial.print("RX power is [dBm] ... "); Serial.println(DW1000Ng::getReceivePower());
//    Serial.print("Signal quality is ... "); Serial.println(DW1000Ng::getReceiveQuality());
//}

void transmit(String msg) {
  // transmit some data
  Serial.print("Transmitting msg: " + msg);
//  String msg = "Hello DW1000Ng, ITS ME::: #"; msg += sentNum;
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
  sentNum++;
  DW1000Ng::clearTransmitStatus();
}
