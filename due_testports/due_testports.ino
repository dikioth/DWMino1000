void setup()
{
  // initialize both serial ports:
  Serial.begin(9600);
  SerialUSB.begin(9600); // Initialize Native USB port
}

void loop()
{
  // read from port 1, send to port 0:
  if (SerialUSB.available())
  {
    int inByte = SerialUSB.read();
    Serial.write(inByte);
  }

  // read from port 0, send to port 1:
  if (Serial.available())
  {
    int inByte = Serial.read();
    SerialUSB.write(inByte);
  }
}
