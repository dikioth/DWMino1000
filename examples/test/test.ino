#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 4; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

volatile boolean received = false;
volatile boolean sent = false;
volatile boolean rxError = false;
String msg;

byte serialBuff[SERIAL_BUFFER_SIZE];

void setup()
{
    Serial.begin(115200);
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
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
    Serial.println("Started");
}

void handleSent()
{
    Serial.println("handled sent");
    sent = true;
}

void handleReceived()
{
    Serial.println("handled received");
    received = true;
}

void handleReceiveFailed()
{
    Serial.println("handled error");
    rxError = true;
}

void transmit()
{
    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData(msg);
    DW1000.startTransmit();
    Serial.println("Transmitted");
}

void receiver()
{
    Serial.println("in actual receive function");
    String rxMsg;
    DW1000.newReceive();
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
    if (Serial.available() > 0)
    {
        int lenSerialBuff = Serial.readBytesUntil('>', serialBuff, SERIAL_BUFFER_SIZE);
        handleSerialReceived(lenSerialBuff);
    }
    else if (received)
    {
        Serial.println("in receive in loop");
        receiver();
        received = false;
    }
}

void clearBuffer()
{
    while (Serial.available())
    {
        Serial.read();
    }
}

void handleSerialReceived(int len)
{
    if (serialBuff[0] == '<')
    {
        // start of command.
        if (serialBuff[1] == 'i')
        {
            // send image.
        }
        else if (serialBuff[1] = 't')
        {
            // send text
        }
    }
}