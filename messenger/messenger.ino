#include <SPI.h>
#include <DW1000.h>

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 4; // irq pin
constexpr uint8_t PIN_SS = 7;  // spi select pin
constexpr uint8_t PIN_LED_RED = 3;
constexpr uint8_t PIN_LED_BLUE = 2;
constexpr uint8_t PIN_LED_YELLOW = 5;

volatile boolean received = false;
volatile boolean sent = false;
volatile boolean rxError = false;

boolean newData = false;
boolean isFlagByteSet = false;
boolean isPrinting = false;
boolean isFlagSet = false;

const int textBytes = 125;
byte textByteArray[textBytes];
byte numReceived = 0;

const int imageBytes = 32 * 1024;
byte imageByteArray[imageBytes];
int imageNumReceived = 0;

byte flagByte;

const int tmpSize = 125;
byte tmpArray[tmpSize];

int line = 1;

boolean setupReceiver = false;
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

  DW1000.attachSentHandler(handleSent);
  DW1000.attachReceivedHandler(handleReceived);
  DW1000.attachReceiveFailedHandler(handleReceiveFailed);

  receiver();
  setupReceiver = true;
}

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
  DW1000.getData(tmpArray, tmpSize);
  delay(10);
  if (setupReceiver) {
    //serialTransmitter(tmpArray);
    printTmpArray();
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

/*** Parse incoming information from Serial (USB) ***/
void serialReceiver()
{
  digitalWrite(PIN_LED_RED, LOW);
  delay(10);
  digitalWrite(PIN_LED_RED, HIGH);
  static byte ndx = 0;
  static int imageNdx = 0;
  static boolean recvInProgress = false;

  byte endMarker = 0x3E; // 0x3E == char '>'
  byte recByte;

  while (Serial.available() > 0 && newData == false)
  {
    if (recvInProgress == true)
    {
      recByte = Serial.read();

      if (!isFlagSet)
      {
        flagByte = recByte;
        isFlagSet = true;
        delay(10);
      }
      if (flagByte == 0x69) // 'i'
      {
        if (recByte != endMarker)
        {
          imageByteArray[imageNdx] = recByte;
          imageNdx++;
          if (imageNdx >= imageBytes)
          {
            imageNdx = imageBytes - 1;
          }
          delayMicroseconds(500);
        }
        else
        {
          recvInProgress = false;
          imageNumReceived = imageNdx + 1;    // save the number for use when printing
          imageByteArray[imageNdx] = recByte; // append end marker to last index
          imageNdx = 0;
          newData = true;
          delayMicroseconds(500);
        }
      }

      else if (flagByte == 0x74) // 't'
      {
        if (recByte != endMarker)
        {
          textByteArray[ndx] = recByte;
          ndx++;
          delayMicroseconds(500);
        }
        else
        {
          recvInProgress = false;
          numReceived = ndx;        // save the number for use when printing
          textByteArray[ndx] = recByte; // append end marker to last index
          ndx = 0;
          newData = true;
          delayMicroseconds(500);
          // serialTransmitter(textByteArray);
        }
      }

    }
    else
    {
      recvInProgress = true;
    }
  }
  digitalWrite(PIN_LED_RED, LOW);
  delayMicroseconds(500);
  //  if (!isPrinting && newData) {
  //    printTextArray();
  //  }
  // uwbTransmitter();
  // showNewData();
}
/*****************************************************/
void printTextArray()
{
  isPrinting = true;
  int n = 0;
  Serial.println("printing textArray");
  while (textByteArray[n] != 0x3E) {
    Serial.print(n);
    Serial.print(": ");
    Serial.println(textByteArray[n]);
    n++;
    delay(1);
  }
  //  for (int n = 0; n < textBytes; n++) {
  //    if (textByteArray[n] == 0x3E) {
  //      Serial.print("Endmarker: ");
  //      Serial.println(textByteArray[n]);
  //      break;
  //    }
  //    Serial.print(n);
  //    Serial.print(": ");
  //    Serial.println(textByteArray[n]);
  //  }
  Serial.print("Endmarker: ");
  Serial.println(textByteArray[n]);
  isPrinting = false;
  clearBuffer();
}

void printTmpArray()
{
  isPrinting = true;
  int n = 0;
  //Serial.println("printing tmpArray");
  while (tmpArray[n] != 0x3E) {
    //Serial.print(n);
    //Serial.print(": ");
    Serial.print(tmpArray[n]);
    n++;
    delay(1);
  }
  //  for (int n = 0; n < textBytes; n++) {
  //    if (textByteArray[n] == 0x3E) {
  //      Serial.print("Endmarker: ");
  //      Serial.println(textByteArray[n]);
  //      break;
  //    }
  //    Serial.print(n);
  //    Serial.print(": ");
  //    Serial.println(textByteArray[n]);
  //  }
  //Serial.print("Endmarker: ");
  Serial.print(tmpArray[n]);
  isPrinting = false;
  clearBuffer();
}

/** Function for transmitting information with the DW1000 **/
void uwbTransmitter()
{
  digitalWrite(PIN_LED_RED, LOW);
  delay(500);
  serialReceiver();
  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(textByteArray, textBytes);
  //   String test = "ttestmsg>";
  //   DW1000.setData(test);
  // switch (flagByte)
  // {
  // case 0x69:
  //     DW1000.setData(imageByteArray, imageNumReceived);
  //     break;

  // case 0x74:
  //     DW1000.setData(textByteArray, numReceived);
  //     break;
  // default:
  //     break;
  // }
  // String msg = "Dummy message";
  // DW1000.setData(msg);
  DW1000.startTransmit();
  newData = false;
  numReceived = 0;
  imageNumReceived = 0;
  isFlagSet = false;
  //  Serial.print("newData: ");
  //  Serial.println(newData);
  digitalWrite(PIN_LED_RED, HIGH);
  // Serial.println("Transmit started");
}

void serialTransmitter(byte data[])
{

  //  Serial.write(data, len);
  //  boolean isTransmitDone = false;
  int i = 0;
  while (data[i] != 0x3E)
  {
    Serial.print(data[i]);
    i++;
    delay(1);
  }
}

//void showNewData()
//{
//  if (newData == true)
//  {
//    //Serial.print("Flagbyte received: ");
//    //Serial.println(flagByte);
//
//    switch (flagByte)
//    {
//      case 0x69:
//        for (int n = 0; n < imageNumReceived; n++)
//        {
//
//          Serial.print(imageByteArray[n], BIN);
//          Serial.print(' ');
//          if ((n + 1) % 4 == 0)
//          {
//            Serial.print("\t: ");
//            Serial.println(line);
//            line++;
//          }
//        }
//        break;
//
//      case 0x74:
//        for (byte n = 0; n < numReceived; n++)
//        {
//
//          Serial.print(textByteArray[n], BIN);
//          Serial.print(' ');
//          if ((n + 1) % 4 == 0)
//          {
//            Serial.print("\t: ");
//            Serial.println(line);
//            line++;
//          }
//        }
//        break;
//    }
//
//    Serial.println();
//    newData = false;
//    numReceived = 0;
//    imageNumReceived = 0;
//    isFlagSet = false;
//  }
//}
//
//void serialTransmitter(byte arr[])
//{
//
//  int n = 0;
//  while (arr[n] != 0x3E)
//  {
//    Serial.print(arr[n], BIN);
//    Serial.print(' ');
//    if ((n + 1) % 4 == 0)
//    {
//      Serial.print("\t: ");
//      Serial.println(line);
//      line++;
//    }
//    n++;
//  }
//  Serial.print("\nend marker: ");
//  Serial.println(arr[n]);
//  Serial.println();
//  newData = false;
//  numReceived = 0;
//  imageNumReceived = 0;
//  isFlagSet = false;
//}

void clearBuffer()
{
  while (Serial.available() > 0)
  {
    Serial.read();
  }
}

/* void receiveMessage(byte flagByte)
  {
    const int messageArrayBytes = 256;
    byte receivedMessageArray[messageArrayBytes];
    receivedMessageArray[0] = flagByte;
    static int ndx = 1;
    byte numReceived = 0;

    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;
    while (Serial.available() > 0 && newData == false)
    {
        Serial.println("reading...");
        recByte = Serial.read();

        if (recvInProgress == true)
        {
            if (recByte != endMarker)
            {
                receivedMessageArray[ndx] = recByte;
                ndx++;
                if (ndx >= messageArrayBytes)
                {
                    // transmit(textByteArray);
                    ndx = messageArrayBytes - 1;
                }
            }
            else
            {
                recvInProgress = false;
                numReceived = ndx + 1;                         // save the number for use when printing
                receivedMessageArray[numReceived] = endMarker; // add endMarker for reading in other arduino
                ndx = 1;
                newData = true;
                Serial.println("receive in progress is done");
            }
        }
    }

    if (recvInProgress == false && newData == true)
    {
        // uwbTransmitter(receivedMessageArray, numReceived);
    }
  }

  void receiveImage(byte flagByte)
  {
    const int imageArrayBytes = 32 * 2048;
    byte receivedImageArray[imageArrayBytes];
    receivedImageArray[0] = flagByte;
    static int ndx = 1;
    byte numReceived = 0;

    static boolean recvInProgress = false;

    byte endMarker = 0x3E; // 0x3E == char '>'
    byte recByte;
    while (Serial.available() > 0 && newData == false)
    {

        recByte = Serial.read();

        if (recvInProgress == true)
        {
            if (recByte != endMarker)
            {
                receivedImageArray[ndx] = recByte;
                ndx++;
                if (ndx >= imageArrayBytes)
                {
                    // transmit(textByteArray);
                    ndx = imageArrayBytes - 1;
                }
            }
            else
            {
                recvInProgress = false;
                numReceived = ndx + 1;                       // save the number for use when printing
                receivedImageArray[numReceived] = endMarker; // add endMarker for reading in other arduino
                ndx = 1;
                newData = true;
            }
        }
    }

    if (recvInProgress == false && newData == true)
    {
        // uwbTransmitter(receivedImageArray, numReceived);
    }
  } */

/*** LOOP ***/
void loop()
{
  /* Route if information is received on the Serial (USB) */
  if (Serial.available() > 0)
  {
    digitalWrite(PIN_LED_RED, HIGH);
    // serialReceiver();
    uwbTransmitter();
    digitalWrite(PIN_LED_RED, LOW);
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
