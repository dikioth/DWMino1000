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

boolean newData = false;
boolean isFlagByteSet = false;
boolean isPrinting = false;
boolean isFlagSet = false;

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

/*** Handlers for DW1000 receive / transmit status ***/
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
/*****************************************************/

/*** Setup DW1000 permanent receive ***/
void receiver()
{
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}
/*****************************************************/

void loop()
{
    /* Route if information is received on the Serial (USB) */
    if (Serial.available() > 0)
    {
        // serialReceiver();
        uwbTransmitter();
        // isPrinting = false;
    }

    /* Route if information is received on the SPI (DW1000) */
    if (received)
    {
        uwbReceiverParser();
    }

    /* Check for errors */
    if (rxError)
    {
        Serial.println("Something went wrong");
    }
}

// const int tmpSize = 32 * 2048;
// byte tmpArray[tmpSize];
/*** Parse incoming information from the UWB (SPI) ***/

void uwbReceiverParser()
{
    Serial.println("uwbReceiverParser");
    int length = DW1000.getDataLength();
    byte tmpArray[length];
    Serial.print("Receiving uwb msg of length: ");
    Serial.println(length);
    String msg;
    DW1000.getData(msg);
    Serial.println(msg);
    // DW1000.getData(tmpArray, length);
    serialTransmitter(tmpArray);
    // showNewData();
    received = false;
}
/*****************************************************/

const byte messageArrayBytes = 32;
byte receivedMessageArray[messageArrayBytes];

const byte textBytes = 32;
byte textByteArray[textBytes];
byte numReceived = 0;

const int imageBytes = 32 * 2048;
byte imageByteArray[imageBytes];
int imageNumReceived = 0;

byte flagByte;

/*** Parse incoming information from Serial (USB) ***/
void serialReceiver()
{
    Serial.println("Serial receiverr");
    static byte ndx = 0;
    static int imageNdx = 0;
    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;

    while (Serial.available() > 0 && newData == false)
    {
        if (recvInProgress == true)
        {
            recByte = Serial.read();

            if (!isFlagSet)
            {
                flagByte = recByte;
                isFlagSet = true;
            }

            if (flagByte == 0x69) // 'i'
            {
                if (recByte != endMarker)
                {
                    imageByteArray[imageNdx] = recByte;
                    imageNdx++;
                    if (imageNdx >= imageBytes)
                    {
                        imageNdx = imageBytes - 1;
                    }
                }
                else
                {
                    recvInProgress = false;
                    imageNumReceived = imageNdx + 1;    // save the number for use when printing
                    imageByteArray[imageNdx] = recByte; // append end marker to last index
                    imageNdx = 0;
                    newData = true;
                }
            }

            if (flagByte == 0x74) // 't'
            {
                if (recByte != endMarker)
                {
                    textByteArray[ndx] = recByte;
                    ndx++;
                    if (ndx >= textBytes)
                    {
                        ndx = textBytes - 1;
                    }
                }
                else
                {
                    recvInProgress = false;
                    numReceived = ndx + 1;        // save the number for use when printing
                    textByteArray[ndx] = recByte; // append end marker to last index
                    ndx = 0;
                    newData = true;
                    serialTransmitter(textByteArray);
                }
            }
        }
        else
        {
            recvInProgress = true;
        }
    }

    uwbTransmitter();
    // showNewData();
}
/*****************************************************/

/** Function for transmitting information with the DW1000 **/
void uwbTransmitter()
{
    DW1000.newTransmit();
    DW1000.setDefaults();
    // DW1000.setData(textByteArray, numReceived);
    String test = "testmsg>";
    DW1000.setData(test);
    // switch (flagByte)
    // {
    // case 0x69:
    //     DW1000.setData(imageByteArray, imageNumReceived);
    //     break;

    // case 0x74:
    //     DW1000.setData(textByteArray, numReceived);
    //     break;
    // default:
    //     break;
    // }
    // String msg = "Dummy message";
    // DW1000.setData(msg);
    DW1000.startTransmit();
    Serial.println("Transmit started");
}

// void serialTransmitter(byte data[], int size)
// {
//     Serial.write(data, BIN);
    // boolean isTransmitDone = false;
    // int i = 0;
    // while (!isTransmitDone)
    // {

    //     if (data[i] == 0x3E)
    //     {
    //         isTransmitDone == true;
    //     }
    //     Serial.write(data[i]);
    //     i++;
    // }
// }

int line = 1;
void showNewData()
{
    if (newData == true)
    {
        Serial.print("Flagbyte received: ");
        Serial.println(flagByte);

        switch (flagByte)
        {
        case 0x69:
            for (int n = 0; n < imageNumReceived; n++)
            {

                Serial.print(imageByteArray[n], BIN);
                Serial.print(' ');
                if ((n + 1) % 4 == 0)
                {
                    Serial.print("\t: ");
                    Serial.println(line);
                    line++;
                }
            }
            break;

        case 0x74:
            for (byte n = 0; n < numReceived; n++)
            {

                Serial.print(textByteArray[n], BIN);
                Serial.print(' ');
                if ((n + 1) % 4 == 0)
                {
                    Serial.print("\t: ");
                    Serial.println(line);
                    line++;
                }
            }
            break;
        }

        Serial.println();
        newData = false;
        numReceived = 0;
        imageNumReceived = 0;
        isFlagSet = false;
    }
    clearBuffer();
}

void serialTransmitter(byte arr[])
{
    int n = 0;
    while (arr[n] != 0x3E)
    {
        Serial.print(arr[n], BIN);
        Serial.print(' ');
        if ((n + 1) % 4 == 0)
        {
            Serial.print("\t: ");
            Serial.println(line);
            line++;
        }
        n++;
    }
    Serial.print("\nend marker: ");
    Serial.println(arr[n]);
    Serial.println();
    newData = false;
    numReceived = 0;
    imageNumReceived = 0;
    isFlagSet = false;

    clearBuffer();
}

void clearBuffer()
{
    while (Serial.available() > 0)
    {
        Serial.read();
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
                    // transmit(textByteArray);
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
        // uwbTransmitter(receivedMessageArray, numReceived);
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
                    // transmit(textByteArray);
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
        // uwbTransmitter(receivedImageArray, numReceived);
    }
}
