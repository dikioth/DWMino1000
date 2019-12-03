#include <SPI.h>
#include <DW1000Ng.hpp>

const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = 7;  // spi select pin

// DEBUG packet sent status and count
volatile unsigned long delaySent = 0;
int16_t sentNum = 0; // todo check int type

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
    Serial.begin(9600);
    DW1000Ng::initializeNoInterrupt(PIN_SS);
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::setDeviceAddress(5);
    DW1000Ng::setNetworkId(10);
    DW1000Ng::setAntennaDelay(16436);
    transmit();
}

void transmit()
{   
    String msg = Serial.readString();
    DW1000Ng::setTransmitData(msg);
    DW1000Ng::startTransmit(TransmitMode::IMMEDIATE);
    delaySent = millis();
    while (!DW1000Ng::isTransmitDone())
    {
    }
    DW1000Ng::clearTransmitStatus();
}

void loop()
{
    if (Serial.available() > 0)
    {
        transmit();
    }
}
