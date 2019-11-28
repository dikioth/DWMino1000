#include "require_cpp11.h"
#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

volatile boolean received = false;
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
    received = true;
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
    Serial.println(msg);

    DW1000.setData(msg);
    DW1000.startTransmit();
    //Serial.print(F("Transmitted: "));
    //  Serial.println("Transmitting: " + msg);
}

void receiver()
{
    byte rxMsg;

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
    // }
}

void loop()
{
    if (Serial.available() > 0)
    {
        transmit();
        clearBuffer();
    }
    else if (received)
    {
        receiver();
        received = false;
        clearBuffer();
    }
} */

// 64000

boolean newData = false;
void setup()
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

boolean isFlagByteSet = false;
void loop()
{
    if (Serial.available() > 0)
    {
        recvBytesWithEndMarkers();
    }
    else if (received)
    {
        receiver();
        received = false;
        clearBuffer();
    }
    // copyToNewArray();
    // showNewData();
}

void handleSent()
{
    sent = true;
}

void handleReceived()
{
    received = true;
}

void handleReceiveFailed()
{
    rxError = true;
}

void recvBytesWithEndMarkers()
{
    
    
    const int messageArrayBytes = 256;
    byte receivedMessageArray[messageArrayBytes];
    receivedMessageArray[0] = flagByte;
    static int ndx = 1;
    
    static boolean recvInProgress = false;
    
    byte numReceived = 0;
    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;
    byte flagByte;
    if (Serial.available() > 0 && !isFlagByteSet)
    {
        flagByte = Serial.read();
        isFlagByteSet = true;
        switch (flagByte)
        {

        case 0x74: // 't'
            receiveMessage(flagByte);
            break;
        case 0x49: // 'i'
            receiveImage(flagByte);
            break;

        default:
        Serial.println("default");
            break;
        }
    }
    
    while (Serial.available() > 0 && newData == false)
    {

        recByte = Serial.read();

        

        if (recvInProgress == true)
        {
            if (recByte != endMarker)
            {
                if (!isFlagByteSet)
                {
                    flagByte = recByte;
                    isFlagByteSet = true;
                }
                receivedBytes[ndx] = recByte;
                ndx++;
                if (ndx >= numBytes)
                {
                    // transmit(receivedBytes);
                    ndx = numBytes - 1;
                }
            }
            else
            {
                // receivedBytes[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                numReceived = ndx + 1; // save the number for use when printing
                ndx = 0;
                newData = true;
            }
        }

        else if (recByte == startMarker)
        {

            recvInProgress = true;
        }
    }
}
void receiveMessage(byte flagByte)
{
    const int messageArrayBytes = 256;
    byte receivedMessageArray[messageArrayBytes];
    receivedMessageArray[0] = flagByte;
    static int ndx = 1;
    byte numReceived = 0;

    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;
    while (Serial.available() > 0 && newData == false)
    {
        Serial.println("reading...");
        recByte = Serial.read();

        if (recvInProgress == true)
        {
            if (recByte != endMarker)
            {
                receivedMessageArray[ndx] = recByte;
                ndx++;
                if (ndx >= messageArrayBytes)
                {
                    // transmit(receivedBytes);
                    ndx = messageArrayBytes - 1;
                }
            }
            else
            {
                recvInProgress = false;
                numReceived = ndx + 1;                         // save the number for use when printing
                receivedMessageArray[numReceived] = endMarker; // add endMarker for reading in other arduino
                ndx = 1;
                newData = true;
                Serial.println("receive in progress is done");
            }
        }
    }

    if (recvInProgress == false && newData == true)
    {
        uwbTransmitter(receivedMessageArray, numReceived);
    }
}

void receiveImage(byte flagByte)
{
    const int imageArrayBytes = 32 * 2048;
    byte receivedImageArray[imageArrayBytes];
    receivedImageArray[0] = flagByte;
    static int ndx = 1;
    byte numReceived = 0;

    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;
    while (Serial.available() > 0 && newData == false)
    {

        recByte = Serial.read();

        if (recvInProgress == true)
        {
            if (recByte != endMarker)
            {
                receivedImageArray[ndx] = recByte;
                ndx++;
                if (ndx >= imageArrayBytes)
                {
                    // transmit(receivedBytes);
                    ndx = imageArrayBytes - 1;
                }
            }
            else
            {
                recvInProgress = false;
                numReceived = ndx + 1;                       // save the number for use when printing
                receivedImageArray[numReceived] = endMarker; // add endMarker for reading in other arduino
                ndx = 1;
                newData = true;
            }
        }
    }

    if (recvInProgress == false && newData == true)
    {
        uwbTransmitter(receivedImageArray, numReceived);
    }
}

void uwbTransmitter(byte data[], int sizeOfData)
{
    boolean transmitDone = false;
    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData(data, sizeOfData);
    DW1000.startTransmit();
    transmitDone = true;
    newData = false;
    //Serial.print(F("Transmitted: "));
    //  Serial.println("Transmitting: " + msg);
}

void receiver()
{

    const int messageArrayBytes = 2048;
    byte receivedMessageArray[messageArrayBytes];

    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
    DW1000.getData(receivedMessageArray, messageArrayBytes);

    if (receivedMessageArray[0] == 0x49 || receivedMessageArray[0] == 0x74)
    {
        serialTransmitter(receivedMessageArray);
    }
}

void serialTransmitter(byte data[])
{
    boolean isTransmitDone = false;
    int i = 0;
    while (!isTransmitDone)
    {

        if (data[i] == 0x3E)
        {
            isTransmitDone == true;
        }
        Serial.print(data[i]);
        i++;
    }
}

/* int line = 1;
void showNewData()
{
    if (newData == true)
    {
        Serial.println("This just in (BIN values)... ");
        // for (byte n = 0; n < numReceived; n++)
        // {
        //     Serial.print(receivedBytes[n], BIN);
        //     Serial.print(' ');
        //     if ((n + 1) % 4 == 0)
        //     {
        //         Serial.print(" :");
        //         Serial.print(line);
        //         Serial.print('\n');
        //         line++;
        //     }
        // }
        Serial.print("First element of receivedBytes: ");
        Serial.println(receivedBytes[0], BIN);
        Serial.print("Last element of receivedBytes: ");
        Serial.println(receivedBytes[numBytes - 1], BIN);
        newData = false;
    }
} */

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
