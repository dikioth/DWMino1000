#include <deca_device_api.h>
#include <deca_param_types.h>
#include <deca_regs.h>
#include <deca_types.h>
#include <deca_version.h>

#include <SPI.h>
#include <DW1000.h>

// OBS: ARDUINO UNO is: RST=8, IRQ=2  and SS=7.
constexpr uint8_t PIN_RST = 8;
constexpr uint8_t PIN_IRQ = 2;
constexpr uint8_t PIN_SS = 7;

volatile boolean received = false;
volatile boolean sent = false;
volatile boolean rxError = false;

boolean newData = false;
boolean setupReceiver = false;

const int SMALL_ARRAY_SIZE = 125;
const int RECEIVE_ARRAY_SIZE = 127;

byte receiveArray[RECEIVE_ARRAY_SIZE];
byte smallArray[SMALL_ARRAY_SIZE];

/*** SETUP ***/
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

    clearBuffer();
    delay(300);

    receiver();
    setupReceiver = true;
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

/*** Setup DW1000 permanent receive ***/
void receiver()
{
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

/*** Parse incoming information from the UWB (SPI) ***/
void uwbReceiver()
{
    DW1000.getData(receiveArray, SMALL_ARRAY_SIZE);
    serialTransmitter();
    received = false;
}

/*** PRINT TO APP THORUGH SERIAL PORT ***/
void serialTransmitter()
{

    byte endMarker = 0x3E;
    int n = 0;
    while (receiveArray[n] != endMarker && n < SMALL_ARRAY_SIZE)
    {
        Serial.print(receiveArray[n], HEX);
        n++;
        delayMicroseconds(100);
    }

    if (receiveArray[n] == endMarker)
    {
        Serial.print(char(endMarker));
        delayMicroseconds(100);
        clearBuffer();
    }
}

void uwbTransmitter()
{
    DW1000.newTransmit();
    DW1000.setDefaults();

    DW1000.setData(smallArray, SMALL_ARRAY_SIZE);
    DW1000.startTransmit();
}

void serialReceiver()
{
    static boolean recvInProgress = false;
    byte endMarker = 0x3E;

    while (Serial.available() > 0 && !newData)
    {
        if (recvInProgress == true)
        {
            int len = Serial.readBytes(smallArray, SMALL_ARRAY_SIZE);
        }

        else
        {
            recvInProgress = true;
        }
    }
    uwbTransmitter();
    delay(200);
}

void clearBuffer()
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
}

/*** LOOP ***/
void loop()
{
    /* Route if information is received on the Serial (USB) */
    if (Serial.available() > 0)
    {
        serialReceiver();
    }

    /* Route if information is received on the SPI (DW1000) */
    if (received)
    {
        uwbReceiver();
    }

    /* Check for errors */
    else if (rxError)
    {
        delay(1000);
    }
}
