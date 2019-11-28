#include "require_cpp11.h"
#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

volatile boolean recieved = false;
volatile boolean sent = false;
volatile boolean rxError = false;

/* void setup()
{
    Serial.begin(9600);
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
    // String msg = Serial.readString();
    byte msg[128];

    DW1000.newTransmit();
    DW1000.setDefaults();
    int arrSize = Serial.availableForWrite();
    Serial.println("size: " + arrSize);
    Serial.println("msg length: " + msg.length());
    Serial.println(msg);

    DW1000.setData(msg);
    DW1000.startTransmit();
    //Serial.print(F("Transmitted: "));
    //  Serial.println("Transmitting: " + msg);
}

void receiver()
{
    String rxMsg;

    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
    DW1000.getData(rxMsg);
    DW1000.readBytes char arr[128];
    Serial.readBytes(arr, 128);
    Serial.println("bytes:");

    // DW1000.getData(rxMsg);
    //  Serial.print(F("Receving..."));

    // if (rxMsg != NULL && rxMsg != "" && rxMsg != "n/a")
    // {
    //     Serial.println(rxMsg);
    //     //    Serial.println("------");
    //     //    uint32_t api = dwt_apiversion();
    //     //    Serial.println(String(api));
    // }
}

void loop()
{
    if (Serial.available() > 0)
    {
        transmit();
        clearBuffer();
    }
    else if (recieved)
    {
        receiver();
        recieved = false;
        clearBuffer();
    }
} */

const byte numBytes = 128;
byte receivedBytes[numBytes];
byte numReceived = 0;

boolean newData = false;
void setup()
{
    Serial.begin(9600);
    Serial.println("<Arduino is ready>");
}

void loop()
{
    recvBytesWithStartEndMarkers();
    showNewData();
}

void recvBytesWithStartEndMarkers()
{
    static boolean recvInProgress = false;
    static byte ndx = 0;
    byte startMarker = 0x3C;
    byte endMarker = 0x3E;
    byte rb;

    while (Serial.available() > 0 && newData == false)
    {
        rb = Serial.read();

        if (recvInProgress == true)
        {
            if (rb != endMarker)
            {
                receivedBytes[ndx] = rb;
                ndx++;
                if (ndx >= numBytes)
                {
                    ndx = numBytes - 1;
                }
            }
            else
            {
                receivedBytes[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                numReceived = ndx; // save the number for use when printing
                ndx = 0;
                newData = true;
            }
        }

        else if (rb == startMarker)
        {
            recvInProgress = true;
        }
    }
}

int line = 1;
void showNewData()
{
    if (newData == true)
    {
        Serial.println("This just in (BIN values)... ");
        for (byte n = 0; n < numReceived; n++)
        {
            Serial.print(receivedBytes[n], BIN);
            Serial.print(' ');
            if ((n+1) % 4 == 0) {
                Serial.print(" :");
                Serial.print(line);
                Serial.print('\n');
                line++;
            }
        }
        Serial.println();
        newData = false;
    }
}

void clearBuffer()
{
    //  Serial.println("Clearing buffer...");
    if (Serial.available() > 0)
    {
        while (Serial.available() > 0)
        {
            Serial.read();
        }
    }
    else
    {
        //    Serial.println("Serial buffer should be empty now");
    }
}

void printByteArray(byte byteArray[])
{
    for (int i = 0; i < sizeof(byteArray); i++)
    {
        Serial.println(byteArray[i]);
    }
}
