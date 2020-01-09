#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
#include <DW1000NgRanging.hpp>
#include <DW1000NgRTLS.hpp>
#include <DW1000NgTime.hpp>
#include <DW1000NgConstants.hpp>

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
const int ARRAY_SIZE = 125;
const int RECEIVE_ARRAY_SIZE = 127;

// Bytes and byte arrays
byte receiveArray[RECEIVE_ARRAY_SIZE];
byte dataArray[ARRAY_SIZE];
int DATA_ARRAY_NUM_RECEIVED = 0;
int LARGE_ARRAY_RECEIVED = 0;

byte flagByte;
typedef unsigned char uint8;

/* RANGING VARIABLES */
byte pseudoEUI[8];
char pseudoEUIstring[24];

/* TAG VARS */
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

frame_filtering_configuration_t ANCHOR_FRAME_FILTER_CONFIG = {
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    true /* This allows blink frames */
};

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

/* ANCHOR RANGING VARIABLES */
typedef struct Position
{
    double x;
    double y;
} Position;

Position position_self = {0, 0};
Position position_B = {3, 0};
Position position_C = {3, 2.5};

double range_self;
double range_B;
double range_C;

boolean received_B = false;

byte target_eui[8];
byte tag_shortAddress[] = {0x05, 0x00};

byte anchor_b[] = {0x02, 0x00};
uint16_t next_anchor = 2;
byte anchor_c[] = {0x03, 0x00};

enum RANGINGSTATE
{
    ANCHOR,
    TAG
};

enum DWMSTATE
{
    RANGING,
    MESSENGER,
    CALL
};

/* CHANGE HERE */
RANGINGSTATE RANGING_STATE = ANCHOR;
DWMSTATE DWM_STATE = RANGING;
/*** SETUP ***/
void setup()
{
    Serial.begin(115200);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    //DW1000.begin(PIN_IRQ, PIN_RST);
    //DW1000.select(PIN_SS);
    DW1000Ng::initialize(PIN_SS, PIN_IRQ, PIN_RST);
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.commitConfiguration();
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveFailed);

    /* RANGING CONFIGS USING DW1000Ng */
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);

    if (RANGING_STATE == ANCHOR)
    {
        DW1000Ng::enableFrameFiltering(ANCHOR_FRAME_FILTER_CONFIG);
        DW1000Ng::setPreambleDetectionTimeout(64);
        DW1000Ng::setSfdDetectionTimeout(273);
        DW1000Ng::setReceiveFrameWaitTimeoutPeriod(5000);
        DW1000Ng::setDeviceAddress(1);
    }
    else if (RANGING_STATE == TAG)
    {
        DW1000Ng::enableFrameFiltering(TAG_FRAME_FILTER_CONFIG);
        DW1000Ng::setPreambleDetectionTimeout(15);
        DW1000Ng::setSfdDetectionTimeout(273);
        DW1000Ng::setReceiveFrameWaitTimeoutPeriod(2000);
    }
    DW1000Ng::setEUI("DD:EE:CC:AA:00:00:00:11");
    DW1000Ng::setNetworkId(RTLS_APP_ID);
    DW1000Ng::setAntennaDelay(16436);

    DW1000Ng::applySleepConfiguration(SLEEP_CONFIG);

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

    if (dataArray[0] == '0' && dataArray[1] == '0') // '0' for ID.
    {

        String rspns = "ID:";
        rspns += pseudoEUIstring;
        rspns += '>';
        Serial.println(rspns);
    }

    else if (dataArray[0] == '1' && dataArray[1] == '1') // '1' for distance.
    {
        String rspns = "DS:";
        rspns += range_self;
        rspns += '>';
        Serial.println(rspns);
    }

    else
    {
        uwbTransmitter();
    }
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

void clearDataArr()
{
    for (int i = 0; i < sizeof(dataArray); i++)
    {
        dataArray[i] = 0;
    }
}

void DWMino_createEUI()
{
    /* A Pseudo EUI built from partID and LotID */
    byte lotID[4];
    DW1000.readBytesOTP(0x007, lotID);
    for (int i = 4; i < 7; i++)
    {
        pseudoEUI[i] = lotID[i];
    }

    sprintf(pseudoEUIstring, "DD:EE:CC:AA:%02X:%02X:%02X:%02X", lotID[5], lotID[6], lotID[7], lotID[8]);
}

/*** LOOP ***/
void loop()
{

    if (Serial.available() > 0)
    {
        serialReceiver();
        clearDataArr();
    }

    if (DWM_STATE == RANGING)
    {

        if (RANGING_STATE == ANCHOR)
        {
            handleAnchorState();
        }
        else if (RANGING_STATE == TAG)
        {
            handleTagState();
        }
    }
    else if (DWM_STATE == MESSENGER)
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
}
/*****************************************************/

/* using https://math.stackexchange.com/questions/884807/find-x-location-using-3-known-x-y-location-using-trilateration */
void calculatePosition(double &x, double &y)
{

    /* This gives for granted that the z plane is the same for anchor and tags */
    double A = ((-2 * position_self.x) + (2 * position_B.x));
    double B = ((-2 * position_self.y) + (2 * position_B.y));
    double C = (range_self * range_self) - (range_B * range_B) - (position_self.x * position_self.x) + (position_B.x * position_B.x) - (position_self.y * position_self.y) + (position_B.y * position_B.y);
    double D = ((-2 * position_B.x) + (2 * position_C.x));
    double E = ((-2 * position_B.y) + (2 * position_C.y));
    double F = (range_B * range_B) - (range_C * range_C) - (position_B.x * position_B.x) + (position_C.x * position_C.x) - (position_B.y * position_B.y) + (position_C.y * position_C.y);

    x = (C * E - F * B) / (E * A - B * D);
    y = (C * D - A * F) / (B * D - A * E);
}

void handleAnchorState()
{
    if (DW1000NgRTLS::receiveFrame())
    {
        size_t recv_len = DW1000Ng::getReceivedDataLength();
        byte recv_data[recv_len];
        DW1000Ng::getReceivedData(recv_data, recv_len);

        if (recv_data[0] == BLINK)
        {
            DW1000NgRTLS::transmitRangingInitiation(&recv_data[2], tag_shortAddress);
            DW1000NgRTLS::waitForTransmission();

            RangeAcceptResult result = DW1000NgRTLS::anchorRangeAccept(NextActivity::RANGING_CONFIRM, next_anchor);
            if (!result.success)
                return;
            range_self = result.range;

            String rangeString = "Range: ";
            rangeString += range_self;
            rangeString += " m";
            rangeString += "\t RX power: ";
            rangeString += DW1000Ng::getReceivePower();
            rangeString += " dBm";
            Serial.println(rangeString);
        }
        else if (recv_data[9] == 0x60)
        {
            double range = static_cast<double>(DW1000NgUtils::bytesAsValue(&recv_data[10], 2) / 1000.0);
            String rangeReportString = "Range from: ";
            rangeReportString += recv_data[7];
            rangeReportString += " = ";
            rangeReportString += range;
            Serial.println(rangeReportString);
            if (received_B == false && recv_data[7] == anchor_b[0] && recv_data[8] == anchor_b[1])
            {
                range_B = range;
                received_B = true;
            }
            else if (received_B == true && recv_data[7] == anchor_c[0] && recv_data[8] == anchor_c[1])
            {
                range_C = range;
                double x, y;
                calculatePosition(x, y);
                String positioning = "Found position - x: ";
                positioning += x;
                positioning += " y: ";
                positioning += y;
                Serial.println(positioning);
                received_B = false;
            }
            else
            {
                received_B = false;
            }
        }
    }
}

void handleTagState()
{
    DW1000Ng::deepSleep();
    delay(blink_rate);
    DW1000Ng::spiWakeup();
    DW1000Ng::setEUI(pseudoEUIstring);

    RangeInfrastructureResult res = DW1000NgRTLS::tagTwrLocalize(1500);
    if (res.success)
        blink_rate = res.new_blink_rate;
}
