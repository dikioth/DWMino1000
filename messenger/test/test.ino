

const int len = 256;
byte bufferArray[len];

boolean myFlag = false;
boolean inProgress = false;
boolean isPrinting = false;
boolean newData = false;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("done setup");

  while (!Serial) {
    // Wait for Serial port to connect
  }

  // Clear buffer
  clearBuffer();
}
int bufferBytesRead;

void loop() {
  // put your main code here, to run repeatedly:
  serialReceiver();
}

byte flagByte;
boolean isFlagSet = false;

void serialReceiver() {
  byte tmpArr[len - 1];
  byte recByte;
  int index = 0;
  while (Serial.available() > 0 && !myFlag) {

    //if ( Serial.available()) {

    recByte = Serial.read();

    if (!isFlagSet)
    {
      flagByte = recByte;
      bufferArray[index] = flagByte;
      index++;
      isFlagSet = true;
      delayMicroseconds(500);
    }
    //bufferBytesRead = Serial.readBytes(tmpArr, len-1);
    else {
      //myFlag = true;
      //newData = true;
      if (recByte == 0x3E) {
        Serial.println("Endbyte");
        bufferArray[index] = recByte;
        inProgress = false;
        myFlag = true;
        newData = true;
        isFlagSet = false;
        delayMicroseconds(500);
      }
      else {
        bufferArray[index] = recByte;
        index++;
        delayMicroseconds(500);
      }
    }
  }
  if (!isPrinting && newData) {
    printArray();
  }
}

void printArray() {
  isPrinting = true;
  int n = 0;
  while (bufferArray[n] != 0x3E) {
    Serial.print(n);
    Serial.print(": ");
    Serial.println(bufferArray[n]);
    n++;
  }
  Serial.print(len - 1);
  Serial.print(": ");
  Serial.println(bufferArray[len - 1]);
  myFlag = false;
  isPrinting = false;
  newData = false;

  // Clear buffer
  clearBuffer();
}

void clearBuffer() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}
