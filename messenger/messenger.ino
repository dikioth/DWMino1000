#include <deca_device_api.h>
#include <deca_param_types.h>
#include <deca_regs.h>
#include <deca_types.h>
#include <deca_version.h>

#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 4; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin
constexpr uint8_t PIN_LED_RED = 3;
constexpr uint8_t PIN_LED_BLUE = 2;
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
const int SMALL_ARRAY_SIZE = 120;
const int RECEIVE_ARRAY_SIZE = 120;


// Bytes and byte arrays
byte receiveArray[SMALL_ARRAY_SIZE];
byte smallArray[SMALL_ARRAY_SIZE];
int SMALL_ARRAY_RECEIVED = 0;
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

    //    int versionnr = dwt_apiversion();
    //    Serial.print("Version: ");
    //    Serial.print(versionnr);

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
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_YELLOW, LOW);
    //    Serial.println("Handle sent");
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
    //serialTransmitter(tmpArray);
    //    Serial.println("uwbReceiver");
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_BLUE, HIGH);
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_BLUE, HIGH);
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_BLUE, HIGH);
    DW1000.getData(receiveArray, SMALL_ARRAY_SIZE);
    serialTransmitter();
    received = false;
}
/*****************************************************/


/*** PRINT ***/
void serialTransmitter()
{
//    Serial.println("serialTransmitter");
    byte endMarker = 0x3E;
    isPrinting = true;
    int n = 0;
    while (receiveArray[n] != endMarker && n < SMALL_ARRAY_SIZE) {
        digitalWrite(PIN_LED_RED, HIGH);
        Serial.print(char(receiveArray[n]));
        n++;
        delayMicroseconds(300);
        digitalWrite(PIN_LED_RED, LOW);
    }
    if (receiveArray[n] == endMarker) {
        Serial.print(char(endMarker));
    }

    isPrinting = false;
    delay(1);
    clearBuffer();
}
/*****************************************************/


/*** Function for transmitting information with the DW1000 ***/
void uwbTransmitter()
{
    digitalWrite(PIN_LED_RED, HIGH);
    DW1000.newTransmit();
    DW1000.setDefaults();
    //    if (isArrayFull) {
    //        Serial.println("suppressing frame check...");
    //        DW1000.suppressFrameCheck(true);
    //    }
    DW1000.setData(smallArray, SMALL_ARRAY_RECEIVED);
    DW1000.startTransmit();
    delay(1000);
    isPartialTransmissionComplete = true;
    isArrayFull = false;

    if (isTransmissionComplete) {
        newData = false;
        SMALL_ARRAY_RECEIVED = 0;
        LARGE_ARRAY_RECEIVED = 0;
        isFlagSet = false;
        flagByte = '0';
        isTransmissionComplete = false;
    }
}
/*****************************************************/


/*** Parse incoming information from Serial (USB) ***/
void serialReceiver()
{
    //    Serial.println("serialReceiver");
    digitalWrite(PIN_LED_RED, HIGH);
    //static byte ndx = 0;
    static int ndx = 0;
    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;

    while (Serial.available() > 0 && !newData && !isPrinting)
    {
        if (recvInProgress == true)
        {
            recByte = Serial.read();

            if (!isFlagSet)
            {
                flagByte = recByte;
                isFlagSet = true;
                delayMicroseconds(500);
            }
            //            if (flagByte == 0x69) // 'i'
            //            {
            //                if (recByte != endMarker)
            //                {
            //                    largeArray[ndx] = recByte;
            //                    ndx++;
            //                    Serial.println("Large array element added");
            //                    if (ndx >= RECEIVE_ARRAY_SIZE)
            //                    {
            //                        ndx = RECEIVE_ARRAY_SIZE - 1;
            //                    }
            //                    delayMicroseconds(500);
            //                }
            //                else
            //                {
            //                    LARGE_ARRAY_RECEIVED = ndx + 1;    // save the number for use when printing
            //                    Serial.println(LARGE_ARRAY_RECEIVED);
            //                    largeArray[ndx] = endMarker; // append end marker to last index
            //                    ndx = 0;
            //                    newData = true;
            //                    recvInProgress = false;
            //                }
            //            }

            //            else if (flagByte == 0x74) // 't'
            //            {
            if (recByte != endMarker)
            {
                smallArray[ndx] = recByte;
                ndx++;
                if (ndx >= SMALL_ARRAY_SIZE) {
                    //                    smallArray[ndx] = endMarker;
                    SMALL_ARRAY_RECEIVED = ndx;
                    isArrayFull = true;
                    Serial.print("received size 2: ");
                    Serial.println(SMALL_ARRAY_RECEIVED);

                    uwbTransmitter();
                    delay(1000);
                    ndx = 0;
                    isPartialTransmissionComplete = false;
                    delayMicroseconds(500);
                }
                //                Serial.println("Small array element added");
                delayMicroseconds(500);
            }
            else
            {
                SMALL_ARRAY_RECEIVED = ndx + 1;        // save the number for use when printing
                Serial.print("received size 3: ");
                Serial.println(SMALL_ARRAY_RECEIVED);
                //                Serial.println(SMALL_ARRAY_RECEIVED);
                smallArray[ndx] = endMarker; // append end marker to last index
                ndx = 0;
                newData = true;
                recvInProgress = false;
                isTransmissionComplete = true;
            }
            //            }
        }
        else
        {
            recvInProgress = true;
        }
    }
    uwbTransmitter();
    digitalWrite(PIN_LED_RED, LOW);
    delay(200);
    clearBuffer();
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
        //        Serial.print("Loop 1, sent value is: ");
        //        Serial.println(sent);
        //        uwbTransmitter();
        serialReceiver();
    }

    /* Route if information is received on the SPI (DW1000) */
    if (received)
    {
        digitalWrite(PIN_LED_BLUE, LOW);
        //        Serial.println("Loop 2, received is true");
        uwbReceiver();
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
