
#include "deca_device_api.h"
#include "deca_regs.h"
#include <deca_spi.h>
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

constexpr uint8_t PIN_RST = 8; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin

/**
 * Application entry point.
 */
void setup()
{
    Serial.begin(115200);

    /* Start with board specific hardware init. */
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
}
/* Loop forever sending frames periodically. */
void loop()
{
    Serial.print("Device ID: ");
    Serial.println(dwt_readdevid(), HEX);
    // Serial.print("Part ID: ");
    // Serial.println(dwt_readtempvbat(0));
    delay(1000);
}

/********************************************************************************
Note 1: For initialisation, DW1000 clocks must be temporarily set to crystal speed. 
After initialisation SPI rate can be increased for optimum performance.
*/