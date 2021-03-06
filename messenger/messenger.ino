#include "require_cpp11.h"
#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 4; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

volatile boolean recieved = false;
volatile boolean sent = false;
volatile boolean rxError = false;
String msg;

void setup()
{
    Serial.begin(9600);
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    DW1000.reset();
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.commitConfiguration();

    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveFailed);
    // DW1000.setGPIOMode(2, 1);
    // DW1000.setGPIOMode(3, 1);
    // DW1000.enableLedBlinking();
    receiver();
}

void handleSent()
{
    sent = true;
}

void handleReceived()
{
    recieved = true;
}

void handleReceiveFailed()
{
    rxError = true;
}

void transmit()
{
    msg = Serial.readString();
    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData(msg);
    DW1000.startTransmit();
    //Serial.print(F("Transmitted: "));
    //Serial.println(msg);
}

void receiver()
{
    String rxMsg;
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
    DW1000.getData(rxMsg);
    //Serial.print(F("Received: "));
    if (rxMsg != NULL && rxMsg != "" && rxMsg != "n/a")
    {
        Serial.println(rxMsg);
    }
}

void loop()
{
    if (Serial.available() > 1)
    {
        transmit();
    }
    else if (recieved)
    {
        receiver();
        recieved = false;
    }
}
