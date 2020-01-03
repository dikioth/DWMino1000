
#include <SPI.h>

/* 
Inpired by: 
    - https://www.arduino.cc/en/Tutorial/SPIEEPROM
    - https://github.com/pbrook/SPIBridge

Registors manipulated are:
OBS: registers only present on AVR boards
- SPIF (SPI Status Register) : can be used to determine whether a transmission is completed. (0->in progress, 1->completed)
- SPCR (SPI Control Register): SPI settings. Serve three purposes, control, data and status. 
- SPDR (SPI Data Register)   : holds the byte which is about to be shifted out the MOSI line, 
                               and the data which has just been shifted in the MISO line. */

/* !Dont forget to connect the SPI pins. */
constexpr uint8_t PIN_RST = 8; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin
byte clr;
void setup()
{
    Serial.begin(115200);
    pinMode(PIN_SS, OUTPUT);
    digitalWrite(PIN_SS, 1);
    Serial.println("HELLOW");
    // SPCR = 01010000
    //interrupt disabled,spi enabled,msb 1st,master,clk low when idle,
    //sample on leading edge of clk,system clock/4 rate (fastest)
    SPCR = (1 << SPE) | (1 << MSTR);
    clr = SPSR;
    clr = SPDR;
    delay(10);
}

void loop()
{
    int data;
    digitalWrite(PIN_SS, LOW);
    for (int i = 0; i < 7; i++)
    {
        data = spi_transfer(0);
        Serial.println(data);
    }
    digitalWrite(PIN_SS, HIGH);
    delay(1000);
}

char spi_transfer(volatile char data)
{
    SPDR = data;                  // Start the transmission
    while (!(SPSR & (1 << SPIF))) // Wait the end of the transmission
    {
    };
    return SPDR; // return the received byte
}

/*
SPCR
| 7    | 6    | 5    | 4    | 3    | 2    | 1    | 0    |
| SPIE | SPE  | DORD | MSTR | CPOL | CPHA | SPR1 | SPR0 |

SPIE - Enables the SPI interrupt when 1
SPE - Enables the SPI when 1
DORD - Sends data least Significant Bit First when 1, most Significant Bit first when 0
MSTR - Sets the Arduino in master mode when 1, slave mode when 0
CPOL - Sets the data clock to be idle when high if set to 1, idle when low if set to 0
CPHA - Samples data on the falling edge of the data clock when 1, rising edge when 0
SPR1 and SPR0 - Sets the SPI speed, 00 is fastest (4MHz) 11 is slowest (250KHz)

NOTE 1: the SPIF bit is cleared by first reading the SPI Status Register with SPIF set, then accessing the 
SPI Data Register (SPDR). p.177 Atmega328p manual. 

*/