#include <deca_device_api.h>
#include <deca_param_types.h>
#include <deca_regs.h>
#include <deca_types.h>
#include <deca_version.h>

#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 8; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

constexpr uint8_t PIN_LED_RED = 3;
constexpr uint8_t PIN_LED_BLUE = 4;
constexpr uint8_t PIN_LED_YELLOW = 5;

// Booleans
volatile boolean received = false;
volatile boolean sent = false;
volatile boolean rxError = false;

boolean newData = false;
boolean isFlagByteSet = false;
boolean isPrinting = false;
boolean isFlagSet = false;
boolean setupReceiver = false;
boolean isPartialTransmissionComplete = false;
boolean isTransmissionComplete = false;
boolean isArrayFull = false;

// Constants
const int ARRAY_SIZE = 125;
const int RECEIVE_ARRAY_SIZE = 127;

// Bytes and byte arrays
byte receiveArray[RECEIVE_ARRAY_SIZE];
byte dataArray[ARRAY_SIZE];
int DATA_ARRAY_NUM_RECEIVED = 0;
int LARGE_ARRAY_RECEIVED = 0;

byte flagByte;

/*** SETUP ***/
void setup()
{
    Serial.begin(115200);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
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
/*****************************************************/

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
    digitalWrite(PIN_LED_BLUE, HIGH);
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}
/*****************************************************/

/*** Parse incoming information from the UWB (SPI) ***/
void uwbReceiver()
{
    digitalWrite(PIN_LED_RED, HIGH);
    //    unsigned long startTime = micros();
    DW1000.getData(receiveArray, ARRAY_SIZE);
    //    unsigned long endTime = micros();
    //    Serial.println("Uwb receiver took: " + String(endTime - startTime) + " microseconds");
    delay(1);
    serialTransmitter();
    received = false;
    digitalWrite(PIN_LED_RED, LOW);
}
/*****************************************************/

/*** PRINT ***/
void serialTransmitter()
{
    int n = 0;
    byte endMarker = 0x3E;
    isPrinting = true;
    //    unsigned long startTime = micros();
    while (n < ARRAY_SIZE)
    {
        if (receiveArray[n] == endMarker)
        {
            if (receiveArray[n + 1] == endMarker && receiveArray[n + 2] == endMarker)
            {
                Serial.print(char(receiveArray[n]));
                delayMicroseconds(500);
                Serial.print(char(receiveArray[n + 1]));
                delayMicroseconds(500);
                Serial.print(char(receiveArray[n + 2]));
                break;
            }
            else
            {
                Serial.print(char(receiveArray[n]));
                n++;
                delayMicroseconds(500);
            }
        }
        else
        {
            Serial.print(char(receiveArray[n]));
            n++;
            delayMicroseconds(500);
        }
    }
    digitalWrite(PIN_LED_RED, LOW);
    //    if (receiveArray[n] == endMarker) {
    //        Serial.println(char(endMarker));
    //    }
    //    unsigned long endTime = micros();
    //    Serial.println("Serial transmitter took: " + String(endTime - startTime) + " microseconds");
    isPrinting = false;
    delayMicroseconds(500);
    clearBuffer();
}
/*****************************************************/

/*** Function for transmitting information with the DW1000 ***/
void uwbTransmitter()
{
    digitalWrite(PIN_LED_RED, HIGH);
    //    unsigned long startTime = micros();
    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData(dataArray, DATA_ARRAY_NUM_RECEIVED);
    DW1000.startTransmit();
    newData = false;
    //    unsigned long endTime = micros();
    //    Serial.println("Uwb transmitter took: " + String(endTime - startTime) + " microseconds");
    delay(1);
    clearBuffer();
    digitalWrite(PIN_LED_RED, LOW);
}
/*****************************************************/

/*** Parse incoming information from Serial (USB) ***/
void serialReceiver()
{
    digitalWrite(PIN_LED_RED, HIGH);
    static int ndx = 0;
    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;
    DATA_ARRAY_NUM_RECEIVED = 0;
    //    unsigned long startTime = micros();

    while (Serial.available() > 0 && !newData && ndx < ARRAY_SIZE && !isPrinting)
    {
        if (recvInProgress == true)
        {
            DATA_ARRAY_NUM_RECEIVED = Serial.readBytes(dataArray, ARRAY_SIZE);
            newData = true;
        }
        else
        {
            recvInProgress = true;
        }
    }
    uwbTransmitter();
}
/*****************************************************/

/*** A function for clearing any remains on the buffer. ***/
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
        digitalWrite(PIN_LED_BLUE, LOW);
        uwbReceiver();
        digitalWrite(PIN_LED_BLUE, HIGH);
    }

    /* Check for errors */
    else if (rxError)
    {
        digitalWrite(PIN_LED_YELLOW, HIGH);
        delay(1000);
        digitalWrite(PIN_LED_YELLOW, LOW);
    }
}
/*****************************************************/
