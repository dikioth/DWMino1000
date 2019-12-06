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

// Constants
const int SMALL_ARRAY_SIZE = 125;
const int LARGE_ARRAY_SIZE = 32 * 1024;


// Bytes and byte arrays
byte largeArray[LARGE_ARRAY_SIZE];
byte smallArray[SMALL_ARRAY_SIZE];
int SMALL_ARRAY_RECEIVED = 0;
int LARGE_ARRAY_RECEIVED = 0;

byte flagByte;

const int tmpSize = 125;
byte tmpArray[tmpSize];

int line = 1;

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
  
  int versionnr = dwt_apiversion();
  Serial.print("Version: ");
  Serial.print(versionnr);

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
  DW1000.getData(largeArray, LARGE_ARRAY_SIZE);
  delay(10);
  if (setupReceiver) {
    //serialTransmitter(tmpArray);
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_BLUE, HIGH);
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_BLUE, HIGH);
    digitalWrite(PIN_LED_BLUE, LOW);
    serialTransmitter();
  }
  received = false;
}
/*****************************************************/


/*** Parse incoming information from the UWB (SPI) ***/
void uwbReceiverParser()
{
  // String msg;
  // DW1000.getData(msg);
  // Serial.println(msg);

  // serialTransmitter(tmpArray);
  // showNewData();
  // delay(1000);
  // Serial.println(msg);
}
/*****************************************************/


/*** PRINT ***/
void serialTransmitter()
{
  if (!setupReceiver) {
    return;
  }
  byte endMarker = 0x3E;
  isPrinting = true;
  int n = 0;
  while (largeArray[n] != endMarker) {
    digitalWrite(PIN_LED_RED, HIGH);
    Serial.print(char(largeArray[n]));
    n++;
    delayMicroseconds(500);
    digitalWrite(PIN_LED_RED, LOW);
  }
  Serial.print(endMarker); // Make sure to also write the end flag
  isPrinting = false;
  delay(2);
  clearBuffer();
}
/*****************************************************/


/*** Function for transmitting information with the DW1000 ***/
void uwbTransmitter()
{
  digitalWrite(PIN_LED_RED, LOW);
  serialReceiver();
  DW1000.newTransmit();
  DW1000.setDefaults();
  switch (flagByte)
  {
    case 0x69:
      DW1000.setData(largeArray, LARGE_ARRAY_RECEIVED);
      break;

    case 0x74:
      DW1000.setData(smallArray, SMALL_ARRAY_RECEIVED);
      break;
  }
  DW1000.startTransmit();
  delay(2);
  newData = false;
  SMALL_ARRAY_RECEIVED = 0;
  LARGE_ARRAY_RECEIVED = 0;
  isFlagSet = false;
  flagByte = '0';
  clearBuffer();
}
/*****************************************************/


/*** Parse incoming information from Serial (USB) ***/
void serialReceiver()
{
  if (!setupReceiver) {
    return;
  }
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
        Serial.println(flagByte);
        delay(10);
      }
      if (flagByte == 0x69) // 'i'
      {
        if (recByte != endMarker)
        {
          largeArray[ndx] = recByte;
          ndx++;
          Serial.println("Large array element added");
          if (ndx >= LARGE_ARRAY_SIZE)
          {
            ndx = LARGE_ARRAY_SIZE - 1;
          }
          delayMicroseconds(500);
        }
        else
        {
          LARGE_ARRAY_RECEIVED = ndx + 1;    // save the number for use when printing
          Serial.println(LARGE_ARRAY_RECEIVED);
          largeArray[ndx] = endMarker; // append end marker to last index
          ndx = 0;
          newData = true;
          recvInProgress = false;
        }
      }

      else if (flagByte == 0x74) // 't'
      {
        if (recByte != endMarker)
        {
          smallArray[ndx] = recByte;
          ndx++;
          Serial.println("Small array element added");
          delayMicroseconds(500);
        }
        else
        {
          SMALL_ARRAY_RECEIVED = ndx + 1;        // save the number for use when printing
          Serial.println(SMALL_ARRAY_RECEIVED);
          smallArray[ndx] = endMarker; // append end marker to last index
          ndx = 0;
          newData = true;
          recvInProgress = false;
        }
      }

    }
    else
    {
      recvInProgress = true;
    }
  }
  digitalWrite(PIN_LED_RED, LOW);
}
/*****************************************************/


/*** LOOP ***/
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
    //digitalWrite(PIN_LED_RED, HIGH);
    // serialReceiver();
    uwbTransmitter();
    //digitalWrite(PIN_LED_RED, LOW);
    // isPrinting = false;
  }

  /* Route if information is received on the SPI (DW1000) */
  else if (received)
  {
    digitalWrite(PIN_LED_BLUE, LOW);
    receiver();
    received = false;
    clearBuffer();
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
