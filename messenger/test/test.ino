

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
int numReceived = 0;

void serialReceiver() {

  byte recByte;
  int index = 0;
  while (Serial.available() > 0 && !myFlag) {
    Serial.println("Inside while");
    //if ( Serial.available()) {

    recByte = Serial.read();

    if (!isFlagSet)
    {
      flagByte = recByte;
      isFlagSet = true;
      Serial.println(flagByte);
      delayMicroseconds(500);
    }
    //bufferBytesRead = Serial.readBytes(tmpArr, len-1);

    //myFlag = true;
    //newData = true;
    if (recByte != 0x3E) {
      bufferArray[index] = recByte;
      index++;
      delayMicroseconds(500);
    }
    else {
      bufferArray[index] = recByte; // Append end marker
      numReceived = index + 1;
      inProgress = false;
      myFlag = true;
      newData = true;
      isFlagSet = false;
      delayMicroseconds(500);
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
  Serial.print("Num received: ");
  Serial.println(numReceived);
  myFlag = false;
  isPrinting = false;
  newData = false;
  numReceived = 0;
  // Clear buffer
  clearBuffer();
}

void clearBuffer() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}
