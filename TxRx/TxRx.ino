/*
 * Copyright (c) 2015 by Thomas Trojer <thomas@trojer.net>
 * Decawave DW1000 library for arduino.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file BasicReceiver.ino
 * Use this to test simple sender/receiver functionality with two
 * DW1000. Complements the "BasicSender" example sketch.
 * 
 * @todo
 *  - move strings to flash (less RAM consumption)
 *  
 */

#include <SPI.h>
#include <DW1000.h>

// connection pins
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = 7;  // spi select pin

// DEBUG packet sent status and count
boolean sent = false;
volatile boolean sentAck = false;
volatile unsigned long delaySent = 0;
DW1000Time sentTime;

// DEBUG packet sent status and count
volatile boolean received = false;
volatile boolean error = false;
String message;

void setup()
{
  // DEBUG monitoring
  Serial.begin(9600);
  Serial.println(F("DWM1000 ARDUINO SENDER AND RECIEVER TEST"));

  // initialize the driver
  DW1000.begin(PIN_IRQ, PIN_RST); // FOR ESP8266.
  DW1000.select(PIN_SS);
  Serial.println(F("DW1000 initialized ..."));

  // general configuration
  DW1000.newConfiguration();
  DW1000.setDefaults();
  DW1000.setDeviceAddress(5); // 6 for other device
  DW1000.setNetworkId(10);
  DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
  DW1000.commitConfiguration();
  Serial.println(F("Committed configuration ..."));

  // attach callback for (successfully) sent messages
  DW1000.attachSentHandler(handleSent);

  // attach callback for (successfully) received messages
  DW1000.attachReceivedHandler(handleReceived);
  DW1000.attachReceiveFailedHandler(handleError);
  DW1000.attachErrorHandler(handleError);
}

void loop()
{
  // HANDLING RECIEVING
  // enter on confirmation of ISR status change (successfully received)
  if (received)
  {
    // get data as string
    DW1000.getData(message);
    Serial.print("Recieved: ... ");
    Serial.println(message);
    received = false;
  }
  if (error)
  {
    Serial.println("Error receiving a message");
    error = false;
    DW1000.getData(message);
    Serial.print("Error data is ... ");
    Serial.println(message);
  }

  // HANDLING SENDING
  if (Serial.available() > 0)
  {
    String msg = Serial.readString();
    transmitter(msg);
  }
}

void transmitter(String msg)
{
  // transmit some data
  Serial.print("Transmitting message ...");

  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(msg);
  // delay sending the message for the given amount
  DW1000Time deltaTime = DW1000Time(10, DW1000Time::MILLISECONDS);
  DW1000.setDelay(deltaTime);
  DW1000.startTransmit();
  delaySent = millis();
}

void receiver()
{
  DW1000.newReceive();
  DW1000.setDefaults();
  // so we don't need to restart the receiver manually
  // DW1000.receivePermanently(true);
  DW1000.startReceive();
}

void handleSent()
{
  // status change on sent success
  sentAck = true;
}

void handleReceived()
{
  // status change on reception success
  received = true;
}

void handleError()
{
  error = true;
}
