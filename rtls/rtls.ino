#include "deca_device_api.h"
#include "deca_regs.h"
#include "deca_spi.h"
#include <SPI.h>

static uint32 status_reg = 0;
#define RX_BUF_LEN 30
typedef unsigned long long uint64;

/* BLINK FRAME DEFINES */
#define BLINK_FRAME_SN_IDX 1 /* Index to access the sequence number of the blink frame in the tx_msg array. */
#define BLINK_DELAY_MS 1000  /* Default inter-blinkframe delay period, in milliseconds. */
uint8 FRAME_SEQ_NUM = 0;

static boolean FRAME_RECIEVED = false;

/* Buffer to store received frame. See NOTE 4 below. */
#define FRAME_LEN_MAX 127
static uint8 rx_buffer[FRAME_LEN_MAX];

/* Declaration of callbacks for interrupts. */
// static void rx_ok_cb(const dwt_cb_data_t *cb_data);
// static void rx_to_cb(const dwt_cb_data_t *cb_data);
// static void rx_err_cb(const dwt_cb_data_t *cb_data);
// static void tx_conf_cb(const dwt_cb_data_t *cb_data);

/* SPI PINS */
constexpr uint8_t PIN_RST = 8; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

/* SERIAL COMMANDS */
static byte serialCommand[FRAME_LEN_MAX];
#define SERIAL_GETID_COMMAND 48 // ASCII: '0'
#define SERIAL_SENDBLINK_COMMAND 49
#define SERIAL_STATEMESSAGE_COMMAND 50
#define SERIAL_STATEIMAGE_COMMAND 51
#define SERIAL_STATEAUDIO_COMMAND 52
#define SERIAL_STATECALL_COMMAND 52

/* FRAME TYPES */
// BLINK FRAME
static uint8 blink_msg[12];
static uint8 range_init_msg[21];
static uint8 poll_msg[21];
static uint8 response_msg[25];
static uint8 final_msg[30];

#define FC_BLINK 0xC5              // Frame control for a Blink.
#define BLINK_DEVID_OFFSET_START 2 // start at second octet.
#define BLINK_DEVID_LENGTH_END 10  // 8 octets.

#define FC_RANGE_INIT 0x20
#define FC_POLL 0x61
#define FC_RESPONSE 0x50
#define FC_FINAL 0x69

static uint16 pan_id = 0xDECA;
uint8 deca_eui[8];
boolean rangingInProgress;

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 �s and 1 �s = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Delay between frames, in UWB microseconds. See NOTE 4 below. */
/* This is the delay from Frame RX timestamp to TX reply timestamp used for calculating/setting the DW1000's delayed TX function. This includes the
 * frame length of approximately 2.46 ms with above configuration. */
#define POLL_RX_TO_RESP_TX_DLY_UUS 2600
/* This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature. */
#define RESP_TX_TO_FINAL_RX_DLY_UUS 500
/* Receive final timeout. See NOTE 5 below. */
#define FINAL_RX_TIMEOUT_UUS 3300
/* Preamble timeout, in multiple of PAC size. See NOTE 6 below. */
#define PRE_TIMEOUT 8

/* Timestamps of frames transmission/reception.
 * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
typedef signed long long int64;
typedef unsigned long long uint64;
static uint64 poll_rx_ts;
static uint64 resp_tx_ts;
static uint64 final_rx_ts;

/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

/* Default antenna delay values for 64 MHz PRF. See NOTE 1 below. */
#define TX_ANT_DLY 16436
#define RX_ANT_DLY 16436
/* This is the delay from Frame RX timestamp to TX reply timestamp used for calculating/setting the DW1000's delayed TX function. This includes the
 * frame length of approximately 2.66 ms with above configuration. */
#define RESP_RX_TO_FINAL_TX_DLY_UUS 3100

/* Hold copies of computed time of flight and distance here for reference so that it can be examined at a debug breakpoint. */
static double tof;
static double distance;

/* String used to display measured distance on LCD screen (16 characters maximum). */
char dist_str[16] = {0};

/* UWI state machine */
enum dwmstates
{
    STATE_INIT,
    STATE_ANCHOR,
    STATE_TAG,
    BLINKSTATE,
    RANGESTATE,
    MESSAGESTATE,
    IMAGESTATE,
    AUDIOSTATE,
    CALLSTATE
};

/* Default UWI state */
dwmstates DWMSTATE = STATE_INIT;

uint32 decaID; // 0xDECA103
uint64_t uwiID;
uint8 newDevID[8];
uint8 pseudoEUI[8];

#define MAX_DEVICES_ALLOWED 10
#define UWI_ID_LENGTH 8
uint8 uwiNeighborhood[10][8]; // Max 10 devices. Each device has 64-Bit ID (8 octets).

void setup()
{
    Serial.begin(115200);

    /* Start with board specific SPI init. */
    DWMino_begin(PIN_SS, PIN_RST, PIN_IRQ);
    DWMino_setdefaults();

    /* Register call-back. */
    dwt_setcallbacks(&tx_conf_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb);
    /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
    dwt_setinterrupt(DWT_INT_TFRS | DWT_INT_RFCG | DWT_INT_RFTO | DWT_INT_RXPTO | DWT_INT_RPHE | DWT_INT_RFCE | DWT_INT_RFSL | DWT_INT_SFDT, 1);

    /* Construction the 64-bit Uwi ID consisting of PartID and LotID */
    dwt_setpanid(pan_id);

    DWMino_createEUI();
    dwt_seteui(pseudoEUI);
    dwt_rxenable(DWT_START_RX_IMMEDIATE);
}
/* Loop forever sending and receiving frames periodically. */
void loop()
{
    /* Wait for any Serial event. */
    if (Serial.available() > 0)
    {
        Serial.readBytesUntil('\n', serialCommand, FRAME_LEN_MAX);
        handleSerialCommand();
    }

    handleStates();
}

void handleSerialCommand()
{
    //Serial.println("GOT SERIAL COMMAND");
    // Serial.println(serialCommand[0]);
    if (serialCommand[0] == SERIAL_GETID_COMMAND)
    {
        /* 32-byte from dwt_getlotid() and 32-bit from dwt_getpartid() */
        Serial.println((char *)pseudoEUI);
    }
}

void handleStates()
{
    switch (DWMSTATE)
    {
    case STATE_INIT:
        Serial.println("STATE INIT");
        handleInitState();
        break;

    case STATE_ANCHOR:
        Serial.println("STATE ANCHOR");
        handleAnchorState();
        break;

    case STATE_TAG:
        Serial.println("STATE TAG");
        handleTagState();
        break;

    default:
        break;
    }
}

boolean isDeviceInArray(uint8 newDeviceID)
{
    boolean newfound = true;
    for (int i = 0; i < (MAX_DEVICES_ALLOWED - 1); i++)
    {
        if (memcmp(&uwiNeighborhood[i * 8], &newDeviceID, sizeof(newDeviceID)) == 0)
        {
            newfound = false;
            break;
        }
    }
    return newfound;
}

void handleInitState()
{
    delay(2000);
    if (FRAME_RECIEVED && (rx_buffer[0] == FC_BLINK))
    {
        DWMSTATE = STATE_ANCHOR;
        FRAME_RECIEVED = false;
    }
    else
    {
        DWMSTATE = STATE_TAG;
        FRAME_RECIEVED = false;
    }
}

void handleAnchorState()
{
    /**
    * A Blink from new device has been received.  Attempting to pair. 
    * Steps are 
    * 1. Ranging init.
    * 2. wait for POLL.
    * 3. send a RESPONSE when POLL received.
    * 4. wait for FINAL frame.
    * 5. (OPTIONAL send a response)
    */

    sendRangeInitFrame();
    while (!FRAME_RECIEVED)
    {
    }

    if (FRAME_RECIEVED && (rx_buffer[21] == 0x61))
    {
        /* Poll message = 0x61. */

        poll_rx_ts = get_rx_timestamp_u64();

        /* Set send time for response. See NOTE 9 below. */
        uint64 resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
        dwt_setdelayedtrxtime(resp_tx_time);

        /* Set expected delay and timeout for final message reception. See NOTE 4 and 5 below. */
        dwt_setrxaftertxdelay(RESP_TX_TO_FINAL_RX_DLY_UUS);
        dwt_setrxtimeout(FINAL_RX_TIMEOUT_UUS);

        sendResponseFrame();
        FRAME_RECIEVED = false;
    }

    while (!FRAME_RECIEVED)
    {
    }

    if (FRAME_RECIEVED && rx_buffer[21] == 0x69)
    {
        /* Final message = 0x69 */

        uint32 poll_tx_ts, resp_rx_ts, final_tx_ts;
        uint32 poll_rx_ts_32, resp_tx_ts_32, final_rx_ts_32;
        double Tround1, Tround2, Treply1, Treply2;
        int64 tof_dtu;

        /* Retrieve response transmission and final reception timestamps. */
        uint64 resp_tx_ts = get_tx_timestamp_u64();
        uint64 final_rx_ts = get_rx_timestamp_u64();

        /* Get timestamps embedded in the final message. */
        final_msg_get_ts(&rx_buffer[22], &poll_tx_ts);
        final_msg_get_ts(&rx_buffer[26], &resp_rx_ts);
        final_msg_get_ts(&rx_buffer[30], &final_tx_ts);

        /* Compute time of flight. 32-bit subtractions give correct answers even if clock has wrapped. See NOTE 12 below. */
        poll_rx_ts_32 = (uint32)poll_rx_ts;
        resp_tx_ts_32 = (uint32)resp_tx_ts;
        final_rx_ts_32 = (uint32)final_rx_ts;
        Tround1 = (double)(resp_rx_ts - poll_tx_ts);
        Tround2 = (double)(final_rx_ts_32 - resp_tx_ts_32);
        Treply1 = (double)(final_tx_ts - resp_rx_ts);
        Treply2 = (double)(resp_tx_ts_32 - poll_rx_ts_32);
        tof_dtu = (int64)((Tround1 * Tround2 - Treply1 * Treply2) / (Tround1 + Tround2 + Treply1 + Treply2));

        tof = tof_dtu * DWT_TIME_UNITS;
        distance = tof * SPEED_OF_LIGHT;

        /* Display computed distance on LCD. */
        sprintf(dist_str, "DIST: %3.2f m", distance);
        Serial.println(dist_str);
        FRAME_RECIEVED = false;
    }
}

void handleTagState()
{

    while (!FRAME_RECIEVED && !(rx_buffer[21] == 0x20))
    {
        DwMino_sendBlink();
        delay(1000);
    }

    FRAME_RECIEVED = false;
    // Store device ID and send Poll frame.
    for (int i = 0; i < 7; i++)
    {
        newDevID[i] = rx_buffer[13 + i];
    }

    sendPollFrame();

    while (!FRAME_RECIEVED)
    {
    }

    FRAME_RECIEVED = false;
    if (rx_buffer[21] == 0x50)
    {
        sendFinalMessageFrame();
    }
}

/* DWMino */
void DWMino_begin(uint8_t SS_PIN, uint8_t RST_PIN, uint8_t IRQ_PIN)
{
    /* Start with board specific SPI init. */
    selectspi(SS_PIN, RST_PIN);
    openspi(IRQ_PIN);

    /* SPI rate must be < 3Mhz when initalized. See note 1 */
    reset_DW1000();
    spi_set_rate_low();
    if (dwt_initialise(DWT_READ_OTP_PID | DWT_READ_OTP_LID) == DWT_ERROR)
    {
        Serial.println("INIT FAILED");
        while (1)
        {
        };
    }
    spi_set_rate_high();
}

void DWMino_sendSerialResponse(char sr[])
{
    Serial.println(sr);
}

void DWMino_createEUI()
{

    /* A Pseudo EUI built from partID and LotID */
    uint32 partID = dwt_getpartid();
    uint32 lotID = dwt_getlotid();

    pseudoEUI[0] = partID;       /* 0xef */
    pseudoEUI[1] = partID >> 8;  /* 0xbe */
    pseudoEUI[2] = partID >> 16; /* 0xad */
    pseudoEUI[3] = partID >> 24; /* 0xde */
    pseudoEUI[4] = lotID;        /* 0xef */
    pseudoEUI[5] = lotID >> 8;   /* 0xbe */
    pseudoEUI[6] = lotID >> 16;  /* 0xad */
    pseudoEUI[7] = lotID >> 24;  /* 0xde */

    //*((unsigned int *)uniqueID) = (uint64_t)partID << 32 | lotID;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn DwMino_sendBlink()
 *
 * @brief Send a blink encoded frame as per the ISO/IEC 24730-62:2013 standard. 
 *        It is a 12 octets frame composed of the following fields:
 *        +--------------------+-----------------+----------------+--------------------+
 *        |      1 octet       |     1 octet     |   8 octets     |      2 octets      |
 *        +--------------------+-----------------+----------------+--------------------+
 *        | Frame Control (FC) | Sequence Number | Tag 64-Bit ID  | FCS                |
 *        +--------------------+-----------------+----------------+--------------------+
 *        | 0xC5               | Modulo 256      | LotID + PartID | Appended by DW1000 |
 *        +--------------------+-----------------+----------------+--------------------+
 *
 * @param  none  
 *
 * @return  none
 */
static void DwMino_sendBlink()
{
    blink_msg[0] = FC_BLINK; // 0xC5
    blink_msg[1] = FRAME_SEQ_NUM;
    fill_deviceID(&blink_msg[2], pseudoEUI);

    /* Write frame data to DW1000 and prepare transmission. See NOTE 7 below. */
    dwt_writetxdata(sizeof(blink_msg), blink_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(blink_msg), 0, 0);        /* Zero offset in TX buffer, no ranging. */

    /* Start transmission, indicating that a response is expected so that reception is enabled immediately after the frame is sent. */
    int a = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
    if (a == 0)
    {
        Serial.println("Successfull sent blink frame");
    }
    else
    {
        Serial.println("Error sending blink frame");
    }

    /* Incrementing sequence number modulo 256 */
    FRAME_SEQ_NUM++;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn sendRangeInitFrame()
 *
 * @brief Callback to process TX confirmation events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
void sendRangeInitFrame()
{
    /* ID of new device stored in newDevID. */
    // contaings the response time to be used in the following ranging exchange.

    range_init_msg[0] = 0x41;
    range_init_msg[1] = 0x8C;
    range_init_msg[2] = FRAME_SEQ_NUM;
    range_init_msg[3] = 0xCA;
    range_init_msg[4] = 0xDE;

    fill_deviceID(&range_init_msg[5], newDevID);
    fill_deviceID(&range_init_msg[13], pseudoEUI);
    range_init_msg[21] = 0x20;

    dwt_writetxdata(sizeof(range_init_msg), range_init_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(range_init_msg), 0, 1);             /* Zero offset in TX buffer */
    dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    FRAME_SEQ_NUM++;
}

void sendPollFrame()
{
    /* ID of new device stored in newDevID. */

    poll_msg[0] = 0x41;
    poll_msg[1] = 0x88;
    poll_msg[2] = FRAME_SEQ_NUM;
    poll_msg[3] = 0xCA;
    poll_msg[4] = 0xDE;

    fill_deviceID(&poll_msg[5], newDevID);
    fill_deviceID(&poll_msg[13], pseudoEUI);
    poll_msg[21] = 0x61;

    dwt_writetxdata(sizeof(poll_msg), poll_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(poll_msg), 0, 1);       /* Zero offset in TX buffer */
    int a = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    if (a == 0)
    {
        Serial.println("Successfull sent Poll frame");
    }
    else
    {
        Serial.println("Error sending poll frame");
    }

    FRAME_SEQ_NUM++;
}

void sendResponseFrame()
{
    // ID of new device stored in newDevID. Sends frame without ToF.
    // computes the Treply1 internally.

    poll_msg[0] = 0x41;
    poll_msg[1] = 0x88;
    poll_msg[2] = FRAME_SEQ_NUM;
    poll_msg[3] = 0xCA;
    poll_msg[4] = 0xDE;

    fill_deviceID(&poll_msg[5], newDevID);
    fill_deviceID(&poll_msg[13], pseudoEUI);
    poll_msg[21] = 0x50;

    dwt_writetxdata(sizeof(poll_msg), poll_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(poll_msg), 0, 1);       /* Zero offset in TX buffer */
    int a = dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    if (a == 0)
    {
        Serial.println("Successfull sent Response frame");
    }
    else
    {
        Serial.println("Error sending Response frame");
    }

    FRAME_SEQ_NUM++;
}

void sendFinalMessageFrame()
{
    final_msg[0] = 0x41;
    final_msg[1] = 0x88;
    final_msg[2] = FRAME_SEQ_NUM;
    final_msg[3] = 0xCA;
    final_msg[4] = 0xDE;

    fill_deviceID(&final_msg[5], newDevID);
    fill_deviceID(&final_msg[13], pseudoEUI);
    final_msg[21] = 0x69;

    uint32 final_tx_time;
    int ret;

    /* Retrieve poll transmission and response reception timestamp. */
    uint64 poll_tx_ts = get_tx_timestamp_u64();
    uint64 resp_rx_ts = get_rx_timestamp_u64();

    /* Compute final message transmission time. See NOTE 10 below. */
    final_tx_time = (resp_rx_ts + (RESP_RX_TO_FINAL_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
    dwt_setdelayedtrxtime(final_tx_time);

    /* Final TX timestamp is the transmission time we programmed plus the TX antenna delay. */
    uint64 final_tx_ts = (((uint64)(final_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;

    /* Write all timestamps in the final message. See NOTE 11 below. */
    final_msg_set_ts(&final_msg[22], poll_tx_ts);
    final_msg_set_ts(&final_msg[26], resp_rx_ts);
    final_msg_set_ts(&final_msg[30], final_tx_ts);

    dwt_writetxdata(sizeof(final_msg), final_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(final_msg), 0, 1);        /* Zero offset in TX buffer, ranging. */
    ret = dwt_starttx(DWT_START_TX_DELAYED);
    if (ret == 0)
    {
        Serial.println("Successfull sent final frame");
    }
    else
    {
        Serial.println("Error sending final frame");
    }
    FRAME_SEQ_NUM++;
}

void DWMino_setdefaults()
{
    /* Default communication configuration. We use here EVK1000's default mode (mode 3). */
    static dwt_config_t config = {
        2,               /* Channel number. */
        DWT_PRF_64M,     /* Pulse repetition frequency. */
        DWT_PLEN_1024,   /* Preamble length. Used in TX only. */
        DWT_PAC32,       /* Preamble acquisition chunk size. Used in RX only. */
        9,               /* TX preamble code. Used in TX only. */
        9,               /* RX preamble code. Used in RX only. */
        1,               /* 0 to use standard SFD, 1 to use non-standard SFD. */
        DWT_BR_110K,     /* Data rate. */
        DWT_PHRMODE_STD, /* PHY header mode. */
        (1025 + 64 - 32) /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    };

    /* Configure DW1000. See NOTE 3 below. */
    dwt_configure(&config);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn resp_msg_get_ts()
 *
 * @brief Read a given timestamp value from the response message. 
 *         In the timestamp fields of the response message, the
 *        least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to get
 *         ts  timestamp value
 *
 * @return none
 */
static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts)
{
    int i;
    *ts = 0;
    for (i = 0; i < 4; i++)
    {
        *ts += ts_field[i] << (i * 8);
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_tx_timestamp_u64()
 *
 * @brief Get the TX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_tx_timestamp_u64()
{
    uint8 ts_tab[5];
    uint64 ts = 0;
    int i;
    dwt_readtxtimestamp(ts_tab);
    for (i = 4; i >= 0; i--)
    {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_timestamp_u64()
 *
 * @brief Get the RX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_rx_timestamp_u64()
{
    uint8 ts_tab[5];
    uint64 ts = 0;
    int i;
    dwt_readrxtimestamp(ts_tab);
    for (i = 4; i >= 0; i--)
    {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_set_ts()
 *
 * @brief Fill a given timestamp field in the final message with the given value. In the timestamp fields of the final
 *        message, the least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to fill
 *         ts  timestamp value
 *
 * @return none
 */
static void final_msg_set_ts(uint8 *ts_field, uint64 ts)
{
    int i;
    for (i = 0; i < 4; i++)
    {
        ts_field[i] = (uint8)ts;
        ts >>= 8;
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn fill_deviceID()
 *
 * @brief Fill the device ID field in the Blink message with the given value.T
 * he least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to fill
 *         ts  timestamp value
 *
 * @return none
 */
static void fill_deviceID(uint8 *id_field, const uint8 devideID[])
{
    int i;
    for (i = 0; i < 7; i++)
    {
        id_field[i] = devideID[i];
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_ok_cb()
 *
 * @brief Callback to process RX good frame events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void rx_ok_cb(const dwt_cb_data_t *cb_data)
{
    /* FILTER range frames if destination address match with host device ID*/

    Serial.println("SOME FRAME RECEIVED");
    FRAME_RECIEVED = true;

    int i;
    /* Clear local RX buffer to avoid having leftovers from previous receptions. This is not necessary but is included here to aid reading the RX
     * buffer. */
    for (i = 0; i < FRAME_LEN_MAX; i++)
    {
        rx_buffer[i] = 0;
    }

    /* A frame has been received, copy it to our local buffer. */
    if (cb_data->datalength <= FRAME_LEN_MAX)
    {
        dwt_readrxdata(rx_buffer, cb_data->datalength, 0);
    }

    /* Handle Recieved frame. */
    if (rx_buffer[0] == FC_BLINK)
    {
        // Storing device ID
        for (int i = 0; i < 7; i++)
        {
            newDevID[i] = rx_buffer[2 + i];
        }
        DWMSTATE = STATE_ANCHOR;
    }

    else if (rx_buffer[21] == 0x20 || rx_buffer[21] == 0x61 || rx_buffer[21] == 0x50 || rx_buffer[21] == 0x69)
    {

        DWMSTATE = STATE_TAG;
    }
}

/* Set corresponding inter-frame delay. */
//tx_delay_ms = DFLT_TX_DELAY_MS;

/* TESTING BREAKPOINT LOCATION #1 */

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_to_cb()
 *
 * @brief Callback to process RX timeout events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void rx_to_cb(const dwt_cb_data_t *cb_data)
{
    Serial.println("RECEIVING TIMEOUT");

    /* Set corresponding inter-frame delay. */
    //tx_delay_ms = RX_TO_TX_DELAY_MS;

    /* TESTING BREAKPOINT LOCATION #2 */
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_err_cb()
 *
 * @brief Callback to process RX error events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void rx_err_cb(const dwt_cb_data_t *cb_data)
{
    /* Set corresponding inter-frame delay. */
    //tx_delay_ms = RX_ERR_TX_DELAY_MS;

    /* TESTING BREAKPOINT LOCATION #3 */
    Serial.println("Bad frame received");
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn tx_conf_cb()
 *
 * @brief Callback to process TX confirmation events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
static void tx_conf_cb(const dwt_cb_data_t *cb_data)
{
    /* 
     * This callback has been defined so that a breakpoint can be put here to check it is correctly called but there is actually nothing specific to
     * do on transmission confirmation in this example. Typically, we could activate reception for the response here but this is automatically handled
     * by DW1000 using DWT_RESPONSE_EXPECTED parameter when calling dwt_starttx().
     * An actual application that would not need this callback could simply not define it and set the corresponding field to NULL when calling
     * dwt_setcallbacks(). The ISR will not call it which will allow to save some interrupt processing time. */

    /* TESTING BREAKPOINT LOCATION #4 */
    Serial.println("Transmission correctly done");
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_get_ts()
 *
 * @brief Read a given timestamp value from the final message. In the timestamp fields of the final message, the least
 *        significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to read
 *         ts  timestamp value
 *
 * @return none
 */
static void final_msg_get_ts(const uint8 *ts_field, uint32 *ts)
{
    int i;
    *ts = 0;
    for (i = 0; i < 4; i++)
    {
        *ts += ts_field[i] << (i * 8);
    }
}