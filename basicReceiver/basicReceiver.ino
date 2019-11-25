#include <SPI.h>
#include <DW1000Ng.hpp>

const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = 7;  // spi select pin

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
    Serial.begin(9600);
    DW1000Ng::initializeNoInterrupt(PIN_SS);
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::setDeviceAddress(6);
    DW1000Ng::setNetworkId(10);
    DW1000Ng::setAntennaDelay(16436);
}

void loop()
{
    DW1000Ng::startReceive();
    while (!DW1000Ng::isReceiveDone())
    {
    }
    DW1000Ng::clearReceiveStatus();
    DW1000Ng::getReceivedData(message);
    Serial.println(message);
}
