#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
#include <DW1000NgTime.hpp>
#include <DW1000NgConstants.hpp>
#include <DW1000NgRanging.hpp>
#include <DW1000NgRTLS.hpp>

#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 4; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

constexpr uint8_t PIN_LED_RED = 3;
constexpr uint8_t PIN_LED_BLUE = 2;
constexpr uint8_t PIN_LED_YELLOW = 5;

typedef unsigned char uint8;
uint8 pseudoEUI[8];
char pseudoEUIstring[24];
String diststr;
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
byte transmitMsg[127];

byte flagByte;

/* RANGING: TAG */

volatile uint32_t blink_rate = 200;

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

frame_filtering_configuration_t TAG_FRAME_FILTER_CONFIG = {
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false};

sleep_configuration_t SLEEP_CONFIG = {
    false, // onWakeUpRunADC   reg 0x2C:00
    false, // onWakeUpReceive
    false, // onWakeUpLoadEUI
    true,  // onWakeUpLoadL64Param
    true,  // preserveSleep
    true,  // enableSLP    reg 0x2C:06
    false, // enableWakePIN
    true   // enableWakeSPI
};

enum DWMSTATE
{
    RANGING_STATE,
    MESSENGER_STATE,
    CALL_STATE
};

DWMSTATE DWM_STATE = RANGING_STATE;

/*** SETUP ***/
void setup()
{
    Serial.begin(115200);
    setupTag();
}
/*****************************************************/

/*** Handlers for DW1000 receive / transmit status ***/
// void handleSent()
// {
//     sent = true;
// }
// void handleReceived()
// {
//     Serial.println("Something received");
//     received = true;
// }
// void handleReceiveFailed()
// {
//     rxError = true;
// }
/*****************************************************/

/*** Setup DW1000 permanent receive ***/
// void receiver()
// {
//     digitalWrite(PIN_LED_BLUE, HIGH);
//     DW1000.newReceive();
//     DW1000.setDefaults();
//     DW1000.receivePermanently(true);
//     DW1000.startReceive();
// }
/*****************************************************/

/*** Parse incoming information from the UWB (SPI) ***/
// void uwbReceiver()
// {
//     digitalWrite(PIN_LED_RED, HIGH);
//     //    unsigned long startTime = micros();
//     DW1000.getData(receiveArray, ARRAY_SIZE);
//     //    unsigned long endTime = micros();
//     //    Serial.println("Uwb receiver took: " + String(endTime - startTime) + " microseconds");
//     delay(1);
//     serialTransmitter();
//     received = false;
//     digitalWrite(PIN_LED_RED, LOW);
// }
/*****************************************************/

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
    isPrinting = false;
    delayMicroseconds(500);
    clearBuffer();
}

void clearDataArr()
{
    for (int i = 0; i < sizeof(dataArray); i++)
    {
        dataArray[i] = 0;
    }
}

/*** PRINT ***/
void serialReceiver()
{
    int lenserial = Serial.readBytes(dataArray, (127 - 9));

    if (dataArray[0] == 'S' && dataArray[1] == 'T')
    {
        if (dataArray[2] == '0')
        {
            //Serial.println("RANGING");
            // DW1000Ng::softwareReset();
            // DW1000Ng::reset();
            // setupTag();
            DWM_STATE = RANGING_STATE;
        }
        else if (dataArray[2] == '1')
        {

            //Serial.println("MESSENGER");
            delay(500);
            //DW1000Ng::disableFrameFiltering();
            //setupMessenger();
            // DW1000Ng::spiWakeup();
            // DW1000Ng::setEUI("DD:EE:CC:AA:00:00:00:22");
            DWM_STATE = MESSENGER_STATE;
        }
    }

    else if (dataArray[0] == 'I' && dataArray[1] == 'D')
    {
        Serial.println("IDSTATE");
        // Serial.print("DD:EE:CC:AA:00:00:00:11>>>");
    }

    else if (dataArray[0] == 'T')
    {
        sendTestFrame();
        DW1000NgRTLS::waitForTransmission();
    }

    else
    {
        //transmitValidFrameExample();
        uwbTransmitter(lenserial);
        DW1000NgRTLS::waitForTransmission();
        //clearDataArr();
        // MESSENGER STATE
        // digitalWrite(PIN_LED_RED, HIGH);
        // static int ndx = 0;
        // static boolean recvInProgress = false;

        // byte endMarker = 0x3E; // 0x3E == char '>'
        // byte recByte;
        // DATA_ARRAY_NUM_RECEIVED = 0;
        // //    unsigned long startTime = micros();

        // while (Serial.available() > 0 && !newData && ndx < ARRAY_SIZE && !isPrinting)
        // {
        //     if (recvInProgress == true)
        //     {

        //         DATA_ARRAY_NUM_RECEIVED = Serial.readBytes(dataArray, ARRAY_SIZE);
        //         newData = true;

        //         // Handling states
        //         if (dataArray[0] == 'S' && dataArray[1] == 'T' && dataArray[2] == '0')
        //         {
        //             DW1000.end();
        //             setupTag();
        //             DWM_STATE = RANGING_STATE;
        //             break;
        //         }
        //     }
        //     else
        //     {
        //         recvInProgress = true;
        //     }
        // }

        // uwbTransmitter();
    }
}
/*****************************************************/

/*** Function for transmitting information with the DW1000 ***/
void uwbTransmitter(int lenserial)
{
    //byte rangingReport[] = {0x41, 0x88, 0, 0x9A, 0x60, 0x01, 0x00, 0x05, 0x00, 1, 2};

    transmitMsg[0] = 0x41;
    transmitMsg[1] = 0x88;
    transmitMsg[3] = 0x9A;
    transmitMsg[4] = 0x60;
    transmitMsg[5] = 0x01;
    transmitMsg[6] = 0x00;
    transmitMsg[7] = 0x05;
    transmitMsg[8] = 0x00;

    memcpy(&transmitMsg[9], dataArray, lenserial);

    DW1000Ng::setTransmitData(transmitMsg, (lenserial + 9));
    DW1000Ng::startTransmit();
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

    if (Serial.available() > 0)
    {

        serialReceiver();
    }

    if (DWM_STATE == RANGING_STATE)
    {
        handleTagState();
    }
    else
    {
        //Serial.println("LOOP ST1");
        // MESSENGER STATE
        if (DW1000NgRTLS::receiveFrame())
        {
            size_t recv_len = DW1000Ng::getReceivedDataLength();
            byte recv_data[recv_len];
            DW1000Ng::getReceivedData(recv_data, recv_len);

            // Serial.print("len frame: ");
            // Serial.println(recv_len);

            // Serial.print("First: ");
            // Serial.println(recv_data[0], HEX);

            if (recv_data[0] == 0x41)
            {
                char rec_data_str[recv_len];
                for (int i = 9; i < recv_len; i++)
                {
                    Serial.print((char)recv_data[i]);
                }

                for (int i = 0; i < recv_len; i++)
                {
                    recv_data[i] = 0;
                }
            }

            else
            {
            }
        }

        // /* Route if information is received on the Serial (USB) */

        // /* Route if information is received on the SPI (DW1000) */
        // if (received)
        // {
        //     digitalWrite(PIN_LED_BLUE, LOW);
        //     uwbReceiver();
        //     digitalWrite(PIN_LED_BLUE, HIGH);
        // }

        // /* Check for errors */
        // else if (rxError)
        // {
        //     digitalWrite(PIN_LED_YELLOW, HIGH);
        //     delay(1000);
        //     digitalWrite(PIN_LED_YELLOW, LOW);
        // }
    }
}
/*****************************************************/

void handleTagState()
{
    DW1000Ng::deepSleep();
    delay(blink_rate);
    DW1000Ng::spiWakeup();
    DW1000Ng::setEUI("DD:EE:CC:AA:00:00:00:22");

    RangeInfrastructureResult res = DW1000NgRTLS::tagTwrLocalize(1500);
    if (res.success)
    {
        blink_rate = res.new_blink_rate;
    }
}

void DWMino_createEUI()
{
    /* A Pseudo EUI built from partID and LotID */
    uint8 partID[4];
    uint8 lotID[4];
    DW1000.readBytesOTP(0x006, partID);
    DW1000.readBytesOTP(0x007, lotID);

    for (int i = 0; i < 3; i++)
    {
        pseudoEUI[i] = partID[i];
    }

    for (int i = 4; i < 7; i++)
    {
        pseudoEUI[i] = lotID[i];
    }

    sprintf(pseudoEUIstring, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
            pseudoEUI[0], pseudoEUI[1], pseudoEUI[2], pseudoEUI[3], pseudoEUI[4], pseudoEUI[5], pseudoEUI[6], pseudoEUI[7], pseudoEUI[8]);
}

void setupTag()
{
    DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);
    // general configuration
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::enableFrameFiltering(TAG_FRAME_FILTER_CONFIG);

    DW1000Ng::setEUI("DD:EE:CC:AA:00:00:00:22");

    DW1000Ng::setNetworkId(RTLS_APP_ID);

    DW1000Ng::setAntennaDelay(16436);

    DW1000Ng::applySleepConfiguration(SLEEP_CONFIG);
    DW1000Ng::setPreambleDetectionTimeout(15);
    DW1000Ng::setSfdDetectionTimeout(273);
    DW1000Ng::setReceiveFrameWaitTimeoutPeriod(2000);
}

void sendTestFrame()
{
    byte rangingReport[] = {0x41, 0x88, 0, 0x9A, 0x60, 0x01, 0x00, 0x05, 0x00, 1, 2};
    DW1000Ng::setTransmitData(rangingReport, sizeof(rangingReport));
    DW1000Ng::startTransmit();
}

void transmitValidFrameExample()
{
    byte testmsg[] = {0x41, 0x88, DW1000NgRTLS::increaseSequenceNumber(), 0x01, 0x00, 0x9A, 0x60, 0x01, 0x00, 0x05, 0x00, 0, 0};
    DW1000Ng::setTransmitData(testmsg, sizeof(testmsg));
    DW1000Ng::startTransmit();
}

void setupMessenger()
{
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    //DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    //DW1000.newConfiguration();
    //DW1000.setDefaults();
    //DW1000.setDeviceAddress(1);
    //DW1000.setNetworkId(10);
    // DW1000.commitConfiguration();
    // DW1000.attachSentHandler(handleSent);
    // DW1000.attachReceivedHandler(handleReceived);
    // DW1000.attachReceiveFailedHandler(handleReceiveFailed);
    //clearBuffer();
    delay(300);

    //receiver();
    //setupReceiver = true;
}