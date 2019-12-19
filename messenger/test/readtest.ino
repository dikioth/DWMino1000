

const int len = 256;
byte bufferArray[len];

boolean myFlag = false;
boolean inProgress = false;
boolean isPrinting = false;
boolean newData = false;

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("done setup");
}

int bufferBytesRead;

void loop()
{
    // put your main code here, to run repeatedly:
    if (Serial.available() > 0)
    {

        int index = 0;
        while (Serial.available() > 0 && !myFlag)
        {
            if (inProgress)
            {
                bufferBytesRead = Serial.readBytes(bufferArray, len);

                myFlag = true;
                newData = true;
                inProgress = false;
                //        if (rb == 0x3E) {
                //          arr[index] = rb;
                //          inProgress = false;
                //          myFlag = true;
                //          newData = true;
                //          delay(1);
                //        }
                //        else {
                //          arr[index] = rb;
                //          index++;
                //          delay(1);
                //        }
            }
            else
            {
                inProgress = true;
            }
        }
    }

    if (!isPrinting && newData)
    {
        printArray();
    }
}

void printArray()
{
    isPrinting = true;
    //for (int n = 0; n < len; n++) {
    int n = 0;
    while (bufferArray[n] != 0x3E)
    {
        Serial.print(n);
        Serial.print(": ");
        Serial.println(bufferArray[n], BIN);
        n++;
    }
    //}

    Serial.print("Bytes read: ");
    Serial.println(bufferBytesRead);
    myFlag = false;
    isPrinting = false;
    newData = false;

    // Clear buffer
    while (Serial.available() > 0)
    {
        Serial.read();
    }
}
