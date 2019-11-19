
#include <SPI.h>
#include <DW1000Ng.hpp>

const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = 7;  // spi select pin

// DEBUG packet sent status and count
volatile unsigned long delaySent = 0;
volatile boolean sentAck = false;
volatile boolean receivedAck = false;
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
    PreambleCode::CODE_3};

void setup()
{
  // DEBUG monitoring
  Serial.begin(9600);
  // initialize the driver
  DW1000Ng::initializeNoInterrupt(PIN_SS);

  DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
  //DW1000Ng::applyInterruptConfiguration(DEFAULT_INTERRUPT_CONFIG);

  DW1000Ng::setDeviceAddress(5);
  DW1000Ng::setNetworkId(10);

  DW1000Ng::setAntennaDelay(16436);
  // attach callback for (successfully) sent messages
  DW1000Ng::attachSentHandler(handleSent);
  DW1000Ng::attachReceivedHandler(handleReceived);
  // start a transmission
}

void handleReceived()
{
  receivedAck = true;
}
void handleSent()
{
  sentAck = true;
}

void transmit()
{
  // transmit some data
  String msg = Serial.readString();
  DW1000Ng::setTransmitData("transmissionTest");
  // delay sending the message for the given amount
  DW1000Ng::startTransmit(TransmitMode::IMMEDIATE);
  while (!DW1000Ng::isTransmitDone())
  {
  }
  DW1000Ng::clearTransmitStatus();
}

void receive()
{
  DW1000Ng::startReceive();
  while (!DW1000Ng::isReceiveDone())
  {
  }
  DW1000Ng::clearReceiveStatus();
  // get data as string
  DW1000Ng::getReceivedData(message);
  Serial.println(message);
}

void loop()
{
  while (Serial.available() <= 0)
  {
    receive();
  }
  transmit();
}
