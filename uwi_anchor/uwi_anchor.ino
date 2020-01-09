#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
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
byte dataArray[RECEIVE_ARRAY_SIZE - 9];
byte transmitMsg[127];
int DATA_ARRAY_NUM_RECEIVED = 0;
int LARGE_ARRAY_RECEIVED = 0;

byte flagByte;

/*RANGING: ANCHOR */
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

frame_filtering_configuration_t MESSENGER_FRAME_FILTER_CONFIG = {
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false /* This allows blink frames */
};

enum DWMSTATE
{
    RANGING_STATE,
    MESSENGER_STATE,
};

DWMSTATE DWM_STATE = RANGING_STATE;

/*** SETUP ***/
void setup()
{
    Serial.begin(115200);
    setupAnchor();
    transmitMsg[0] = 0x41;
    transmitMsg[1] = 0x88;
}

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
    isPrinting = false;
    delayMicroseconds(500);
    clearBuffer();
}
/*****************************************************/

/*** Function for transmitting information with the DW1000 ***/
void uwbTransmitter(int lenserial)
{
    DW1000Ng::getNetworkId(&transmitMsg[3]);
    memcpy(&transmitMsg[5], tag_shortAddress, 2);
    DW1000Ng::getDeviceAddress(&transmitMsg[7]);

    int i = 9;
    for (byte bt : dataArray)
    {
        transmitMsg[i] = bt;
        i++;
    }

    DW1000Ng::setTransmitData(transmitMsg, (lenserial + 9));
    DW1000Ng::startTransmit();
}
/*****************************************************/

/*** Parse incoming information from Serial (USB) ***/
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
            // setupAnchor();
            DW1000Ng::enableFrameFiltering(ANCHOR_FRAME_FILTER_CONFIG);
            DWM_STATE = RANGING_STATE;
        }
        else if (dataArray[2] == '1')
        {
            //Serial.println("MESSENGER");
            //DW1000Ng::disableFrameFiltering();
            //DW1000Ng::softwareReset()J;
            //DW1000Ng::reset();
            //delay(500);
            //DW1000Ng::disableFrameFiltering();

            //setupMessenger();
            DW1000Ng::enableFrameFiltering(MESSENGER_FRAME_FILTER_CONFIG);
            DWM_STATE = MESSENGER_STATE;
        }

        clearDataArr();
    }

    else if (dataArray[0] == 'I' && dataArray[1] == 'D')
    {
        Serial.println("IDSTATE");
        // Serial.print("DD:EE:CC:AA:00:00:00:11>>>");
        clearDataArr();
    }

    // else if (dataArray[0] == 'T')
    // {
    //     sendTestFrame();
    //     DW1000NgRTLS::waitForTransmission();
    // }

    else
    {
        //transmitValidFrameExample();
        uwbTransmitter(lenserial);
        DW1000NgRTLS::waitForTransmission();
        //sendTestFrame();
        clearDataArr();
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

/*** LOOP ***/
void loop()
{

    if (Serial.available() > 0)
    {
        serialReceiver();
    }

    if (DWM_STATE == RANGING_STATE)
    {
        handleAnchorState();
    }
    else
    { // Messenger state

        // MESSENGER STATE
        if (DW1000NgRTLS::receiveFrame())
        {
            size_t recv_len = DW1000Ng::getReceivedDataLength();

            byte recv_data[recv_len];
            DW1000Ng::getReceivedData(recv_data, recv_len);
            // Serial.println("Fraem received");
            // Serial.print("LEN: ");
            // Serial.println(recv_len);

            // Serial.print("FIRS: ");
            // Serial.println(recv_data[0]);

            // if (recv_data[0] == 0x41)
            // {
            char rec_data_str[recv_len];
            for (int i = 9; i < recv_len; i++)
            {
                Serial.print((char)recv_data[i]);
            }

            for (int i = 0; i < recv_len; i++)
            {
                recv_data[i] = 0;
            }
            //}
        }
    }
}
/*****************************************************/

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

            String rangeString = "DS:";
            rangeString += range_self;
            Serial.println(rangeString);

            transmitRangeReport();
            DW1000NgRTLS::waitForTransmission();
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

void setupAnchor()
{

    DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);
    // general configuration
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::enableFrameFiltering(ANCHOR_FRAME_FILTER_CONFIG);

    DW1000Ng::setEUI("DD:EE:CC:AA:00:00:00:11");

    DW1000Ng::setPreambleDetectionTimeout(64);
    DW1000Ng::setSfdDetectionTimeout(273);
    DW1000Ng::setReceiveFrameWaitTimeoutPeriod(5000);

    DW1000Ng::setNetworkId(RTLS_APP_ID);
    DW1000Ng::setDeviceAddress(1);

    DW1000Ng::setAntennaDelay(16436);
}

void transmitRangeReport()
{
    byte rangingReport[] = {DATA, SHORT_SRC_AND_DEST, DW1000NgRTLS::increaseSequenceNumber(), 0, 0, 0, 0, 0, 0, 0x60, 0, 0};
    DW1000Ng::getNetworkId(&rangingReport[3]);
    memcpy(&rangingReport[5], tag_shortAddress, 2);
    DW1000Ng::getDeviceAddress(&rangingReport[7]);
    DW1000NgUtils::writeValueToBytes(&rangingReport[10], static_cast<uint16_t>((range_self * 1000)), 2);
    DW1000Ng::setTransmitData(rangingReport, sizeof(rangingReport));
    DW1000Ng::startTransmit();
}

void sendTestFrame()
{
    byte rangingReport[] = {0x41, 0x88, 0, 0x9A, 0x60, 0x05, 0x00, 0x01, 0x00, 'D', 'E'};
    DW1000Ng::setTransmitData(rangingReport, sizeof(rangingReport));
    DW1000Ng::startTransmit();
}

void transmitValidFrameExample()
{
    byte rangingReport[] = {DATA, SHORT_SRC_AND_DEST, DW1000NgRTLS::increaseSequenceNumber(), 'P', 'N', 'D', 'S', 'S', 'C', 'D', 'E', 'C', 'A'};
    DW1000Ng::getNetworkId(&rangingReport[3]);
    memcpy(&rangingReport[5], tag_shortAddress, 2);
    DW1000Ng::getDeviceAddress(&rangingReport[7]);
    DW1000Ng::setTransmitData(rangingReport, sizeof(rangingReport));
    DW1000Ng::startTransmit();
}
void setupMessenger()
{
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    //DW1000.begin(PIN_IRQ, PIN_RST);
    //DW1000.select(PIN_SS);

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