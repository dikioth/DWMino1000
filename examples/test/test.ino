
#include "decadriver\deca_device_api.h"
#include "decadriver\deca_regs.h"
#include "decadriver\deca_spi.h"
#include <SPI.h>

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

/* SPI PINS */
constexpr uint8_t PIN_RST = 8; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

/* Declaration of static functions. */
static void rx_ok_cb(const dwt_cb_data_t *cb_data);
static void rx_to_cb(const dwt_cb_data_t *cb_data);
static void rx_err_cb(const dwt_cb_data_t *cb_data);
static void tx_conf_cb(const dwt_cb_data_t *cb_data);

/* The frame sent in this example is a blink encoded as per the ISO/IEC 24730-62:2013 standard. It is a 14-byte frame composed of the following fields:
 *     - byte 0: frame control (0xC5 to indicate a multipurpose frame using 64-bit addressing).
 *     - byte 1: sequence number, incremented for each new frame.
 *     - byte 2 -> 9: device ID, see NOTE 1 below.
 *     - byte 10: encoding header (0x43 to indicate no extended ID, temperature, or battery status is carried in the message).
 *     - byte 11: EXT header (0x02 to indicate tag is listening for a response immediately after this message).
 *     - byte 12/13: frame check-sum, automatically set by DW1000. */
static uint8 tx_msg[] = {0xC5, 0, 'D', 'E', 'C', 'A', 'W', 'A', 'V', 'E', 0x43, 0x02, 0, 0};
/* Index to access the sequence number of the blink frame in the tx_msg array. */
#define BLINK_FRAME_SN_IDX 1

/* Delay from end of transmission to activation of reception, expressed in UWB microseconds (1 uus is 512/499.2 microseconds). See NOTE 2 below. */
#define TX_TO_RX_DELAY_UUS 60

/* Receive response timeout, expressed in UWB microseconds. See NOTE 3 below. */
#define RX_RESP_TO_UUS 5000

/* Default inter-frame delay period, in milliseconds. */
#define DFLT_TX_DELAY_MS 1000
/* Inter-frame delay period in case of RX timeout, in milliseconds.
 * In case of RX timeout, assume the receiver is not present and lower the rate of blink transmission. */
#define RX_TO_TX_DELAY_MS 3000
/* Inter-frame delay period in case of RX error, in milliseconds.
 * In case of RX error, assume the receiver is present but its response has not been received for any reason and retry blink transmission immediately. */
#define RX_ERR_TX_DELAY_MS 0
/* Current inter-frame delay period.
 * This global static variable is also used as the mechanism to signal events to the background main loop from the interrupt handler callbacks,
 * which set it to positive delay values. */
static int32 tx_delay_ms = -1;

void setup()
{
    Serial.begin(115200);

    /* Start with board specific SPI init. */
    selectspi(PIN_SS, PIN_RST);
    openspi(PIN_IRQ);

    /* SPI rate must be < 3Mhz when initalized. See note 1 */
    reset_DW1000();
    spi_set_rate_low();
    if (dwt_initialise(DWT_LOADNONE) == DWT_ERROR)
    {
        Serial.println("INIT FAILED");
        while (1)
        {
        };
    }
    spi_set_rate_high();

    // /* Configure DW1000. See NOTE 3 below. */
    dwt_configure(&config);

    /* Register RX call-back. */
    dwt_setcallbacks(&tx_conf_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb);

    /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
    dwt_setinterrupt(DWT_INT_TFRS | DWT_INT_RFCG | DWT_INT_RFTO | DWT_INT_RXPTO | DWT_INT_RPHE | DWT_INT_RFCE | DWT_INT_RFSL | DWT_INT_SFDT, 1);

    /* Set delay to turn reception on after transmission of the frame. See NOTE 2 below. */
    dwt_setrxaftertxdelay(TX_TO_RX_DELAY_UUS);

    /* Set response frame timeout. */
    dwt_setrxtimeout(RX_RESP_TO_UUS);
}
/* Loop forever sending frames periodically. */
void loop()
{

    /* Write frame data to DW1000 and prepare transmission. See NOTE 7 below. */
    dwt_writetxdata(sizeof(tx_msg), tx_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(tx_msg), 0, 0);     /* Zero offset in TX buffer, no ranging. */

    /* Start transmission, indicating that a response is expected so that reception is enabled immediately after the frame is sent. */
    dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    /* Wait for any RX event. */
    while (tx_delay_ms == -1)
    {
    };

    /* Execute the defined delay before next transmission. */
    if (tx_delay_ms > 0)
    {
        delay(tx_delay_ms);
    }

    /* Increment the blink frame sequence number (modulo 256). */
    tx_msg[BLINK_FRAME_SN_IDX]++;

    /* Reset the TX delay and event signalling mechanism ready to await the next event. */
    tx_delay_ms = -1;
}

void clearLocalBuffer(*lbuffer)
{
    for (int i = 0; sizeof(lbuffer); i++)
    {
        lbuffer[i] = 0;
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

    /* Set corresponding inter-frame delay. */
    tx_delay_ms = DFLT_TX_DELAY_MS;

    /* TESTING BREAKPOINT LOCATION #1 */
}

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
    /* Set corresponding inter-frame delay. */
    tx_delay_ms = RX_TO_TX_DELAY_MS;

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
    tx_delay_ms = RX_ERR_TX_DELAY_MS;

    /* TESTING BREAKPOINT LOCATION #3 */
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
    /* This callback has been defined so that a breakpoint can be put here to check it is correctly called but there is actually nothing specific to
     * do on transmission confirmation in this example. Typically, we could activate reception for the response here but this is automatically handled
     * by DW1000 using DWT_RESPONSE_EXPECTED parameter when calling dwt_starttx().
     * An actual application that would not need this callback could simply not define it and set the corresponding field to NULL when calling
     * dwt_setcallbacks(). The ISR will not call it which will allow to save some interrupt processing time. */

    /* TESTING BREAKPOINT LOCATION #4 */
}

/********************************************************************************
Note 1: For initialisation, DW1000 clocks must be temporarily set to crystal speed. 
After initialisation SPI rate can be increased for optimum performance.
*/
