/*! ----------------------------------------------------------------------------

 * @file	deca_spi.c
 * @brief	SPI access functions
 *
 * @attention
 *
 * Copyright 2013 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#include "deca_param_types.h"
#include "deca_device_api.h"

const SPISettings fastSPI = SPISettings(16000000L, MSBFIRST, SPI_MODE0);
const SPISettings slowSPI = SPISettings(2000000L, MSBFIRST, SPI_MODE0);
const SPISettings *currentSPI = &fastSPI;

uint8_t _ss;
uint8_t _rst;
uint8_t _irq;

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: selectspi()
 *
 * Low level abstract function to select the DWM1000.
 * Make the pin SS into output pin and set it to HIGH. 
 */
int selectspi(uint8_t pin_ss, uint8_t pin_rst)
{
    _ss = pin_ss;
    _rst = pin_rst;
    pinMode(_ss, OUTPUT);
    digitalWrite(_ss, HIGH);
    pinMode(_rst, INPUT); // ! ALWAYS tristate. MUST NEVER BE PULLED UP. E.g never HIGH.

    return 0;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */

int openspi(uint8_t irq)
{
    pinMode(irq, INPUT);
    delay(5);
    SPI.begin();
    //SPI.usingInterrupt(digitalPinToInterrupt(irq));
    //attachInterrupt(digitalPinToInterrupt(irq), dwt_isr, RISING); //

    return 0;

} // end openspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void)
{
    SPI.end();
    return 0;

} // end closespi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospi()
 *
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 */
int writetospi(uint16 headerLength, const uint8 *headerBuffer, uint32 bodylength, const uint8 *bodyBuffer)
{

    // decaIrqStatus_t stat;
    // stat = decamutexon();
    SPI.beginTransaction(*currentSPI);
    digitalWrite(_ss, LOW);

    for (int i = 0; i < headerLength; i++)
    {
        SPI.transfer(headerBuffer[i]); // send header
    }

    for (int i = 0; i < bodylength; i++)
    {
        SPI.transfer(bodyBuffer[i]); // write values
    }
    delayMicroseconds(5);
    digitalWrite(_ss, HIGH);
    SPI.endTransaction();

    // decamutexoff(stat);

    return 0;
} // end writetospi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: readfromspi()
 *
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data may be found,
 * or returns -1 if there was an error
 */
int readfromspi(uint16 headerLength, const uint8 *headerBuffer, uint32 readlength, uint8 *readBuffer)
{
    // decaIrqStatus_t stat;
    // stat = decamutexon();

    SPI.beginTransaction(*currentSPI);
    digitalWrite(_ss, LOW);
    for (int i = 0; i < headerLength; i++)
    {
        SPI.transfer(headerBuffer[i]); // send header
    }
    for (int i = 0; i < readlength; i++)
    {
        readBuffer[i] = SPI.transfer(0); // read values
    }
    delayMicroseconds(5);
    digitalWrite(_ss, HIGH);
    SPI.endTransaction();
    // decamutexoff(stat);
    return 0;
} // end readfromspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: reset_DW1000()
 * Reset DWM1000. the RSTn pin should not be driven high but left floating. (dw1000 data sheet v2.08 §5.6.1 page 20)
 * DW1000 data sheet v2.08 §5.6.1 page 20: nominal 50ns, to be safe take more time.
 * DW1000 data sheet v1.2 page 5: nominal 3 ms, to be safe take more time
 */
void reset_DW1000()
{
    //!MUST NOT BE PULLED UP.
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(2);
    pinMode(_rst, INPUT);
    delay(10);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: spi_set_rate_low()
 * When INIT state: SPI rate shoud not be greater than 3Mhz. ( TODO: reference to manual).
 */
void spi_set_rate_low()
{
    currentSPI = &slowSPI;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: spi_set_rate_high()
 * When IDLE state: SPI can max be x (TODO: Max SPi rate and reference to manual).
 */
void spi_set_rate_high()
{
    currentSPI = &fastSPI;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: decamutexon()
 *
 * Description: This function should disable interrupts. This is called at the start of a critical section
 * It returns the irq state before disable, this value is used to re-enable in decamutexoff call
 *
 * Note: The body of this function is defined in deca_mutex.c and is platform specific
 *
 * input parameters:	
 *
 * output parameters
 *
 * returns the state of the DW1000 interrupt
 */
decaIrqStatus_t decamutexon(void)
{
    // TODO: return state before disable, value is used to re-enable in decamutexoff call
    detachInterrupt(digitalPinToInterrupt(_irq)); //disable the external interrupt line
    return 1;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: decamutexoff()
 *
 * Description: This function should re-enable interrupts, or at least restore their state as returned(&saved) by decamutexon 
 * This is called at the end of a critical section
 *
 * Note: The body of this function is defined in deca_mutex.c and is platform specific
 *
 * input parameters:	
 * @param s - the state of the DW1000 interrupt as returned by decamutexon
 *
 * output parameters
 *
 * returns the state of the DW1000 interrupt
 */
void decamutexoff(decaIrqStatus_t s)
{
    // TODO: put a function here that re-enables the interrupt at the end of the critical section
    SPI.usingInterrupt(digitalPinToInterrupt(_irq));
    attachInterrupt(digitalPinToInterrupt(_irq), dwt_isr, RISING);
}