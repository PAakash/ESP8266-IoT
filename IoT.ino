/*
  Name : Aakash Patel
  Project : IoT : ESP8266 Webserver with AJAX
  Matriculation Nummer : 255461

  Description :
    Using Arduino with ESP8266 for creating a Webserver
    Using AJAX for updation of website
    Hardware ESP8266-01, Arduino Nano, ADXL 335
    Website with information for 6 Analog channel
    Accelerometer angle information
    Two digital input
    Five PWM Output
    Three digital Output
    Accelerometer updation on Thingspeak

  LED identification
  RED - Setting up ESP
  GREEN - ESP Setup successful
  BLUE - Webserver Active
*/

// Include homepage of the webserver
#include "html.h"
#include <SoftwareSerial.h>
#include <math.h>

// Accelerometer constants
#define ADC_ref 5.00
#define zero_x 1.569
#define zero_y 1.569
#define zero_z 1.569
#define sensitivity_x 0.3
#define sensitivity_y 0.3
#define sensitivity_z 0.3

float angle_x, angle_y, angle_z;

// Defining ESP8266 connection at pin 2,3
SoftwareSerial esp8266(2, 3); // RX, TX

// Debugging mode
#define DEBUG true

// SSID and Password for AP
#define SSID "Aakash"
#define PASSWORD "038{eP17"

// Variable for pinstatus
int dac[] = {0, 0, 0, 0, 0};
int DigitalPin[] = {4, 7 , 8 , 12 , 13};
int PwmPin[] = {5, 6, 9, 10, 11};

// Host address for Thingspeak
String Host = "184.106.153.149";

String xSend;

// Maximum retry
int Mretry = 10;

void setup() {

  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  esp8266.begin(57600);
  Serial.begin(57600);

  red();

  // Configure ESP
  if (!espConfig()) serialDebug();
  else {
    green();
    getIP();
    digitalWrite(13, HIGH);
  }

  // Config ESP TCP server
  if (configTCPServer()) {
    blue();
    debug(F("Server Aktiv"));
  }
  else debug(F("Server Error"));

  delay(20);
}

void loop() {
  if (esp8266.available())
  {
    // If any incoming request
    if (esp8266.find("+IPD,"))
    {
      // Read connection ID
      int connectionId = esp8266.parseInt();

      // Request GET,OUT,POST etc
      String command1 = esp8266.readStringUntil('/');
      // Request Data initials
      String command = esp8266.readStringUntil('/');
      debug(command);
      // String for response
      String a;

      // If digital I/O requested
      if (command == "digital") {

        int pin, value;
        pin = esp8266.parseInt();

        if (esp8266.read() == '/') {
          value = esp8266.parseInt();
          digitalWrite(pin, value);
        }
        else {
          value = digitalRead(pin);
        }

        // Creating response for digital I/O request
        a = headerXML;
        a += F("digital,");
        a += String(pin);
        a += F(",");
        a += String(value);
        a += "";
        a += "\r\n";

        // Send response
        sendWebsite(connectionId, a);
        // Closr connection
        espClose(connectionId, Mretry);

        a = "";
      }
      // If dac or PWM requested
      else if (command == "dac") {
        int pin, value;
        pin = esp8266.parseInt();
        if (esp8266.read() == '/') {
          value = esp8266.parseInt();

          if (pin == 5)
            dac[0] = value;
          else if (pin == 6)
            dac[1] = value;
          else if (pin == 9)
            dac[2] = value;
          else if (pin == 10)
            dac[3] = value;
          else if (pin == 11)
            dac[4] = value;

          analogWrite(pin, value);
        }
        else {

          if (pin == 5)
            value = dac[0];
          else if (pin == 6)
            value = dac[1];
          else if (pin == 9)
            value = dac[2];
          else if (pin == 10)
            value = dac[3];
          else if (pin == 11)
            value = dac[4];
        }

        // Creating DAC response
        a = headerXML;
        a += F("dac,");
        a += String(pin);
        a += F(",");
        a += String(value);
        a += "";
        a += "\r\n";
        // Send response
        sendWebsite(connectionId, a);
        // Close connection
        espClose(connectionId, Mretry);

        a = "";
      }
      // If status requested
      else if (command == "status") {

        // Creating response for status with seperator '#'
        // i.e status#PIN=VALUE
        a = headerXML;
        int pin, value;
        a += F("status");

        for (int thisPin = 0; thisPin < 5; thisPin++) {
          pin = DigitalPin[thisPin];
          value = digitalRead(pin);

          a += F("#");
          a += String(pin);
          a += F("=");
          a += String(value);

        }
        {
          for (int thisPin = 0; thisPin < 5; thisPin++) {

            pin = PwmPin[thisPin];
            value = dac[thisPin];

            a += F("#");
            a += String(pin);
            a += F("=");
            a += String(value);
          }
          {
            for (int thisPin = 0; thisPin < 5; thisPin++) {
              value = analogRead(thisPin);

              a += "#A" + String(thisPin);
              a += F("=");
              a += String(value);
            }

            // Calculate angle from ADXL data
            calcAngle();

            a += "#A" + String(5);
            a += F("=");
            a += String(angle_z);

            a += "#A" + String(6);
            a += F("=");
            a += String(angle_y);

            a += "#A" + String(7);
            a += F("=");
            a += String(angle_x);
          }
          a += "";
          a += "\r\n";

          // Send response
          sendWebsite(connectionId, a);
          // Close connection
          espClose(connectionId, Mretry);

          a = "";
        }
      }
      // If thingspeak requested
      else if (command == "ts") {
        // Send TS request
        sendTSData(connectionId);
        // Create response for server
        a = headerXML;
        a += F("TS");
        a += "";
        a += "\r\n";
        // Response to server
        sendWebsite(connectionId, a);
        // Close connection
        espClose(connectionId, Mretry);

        a = "";
      }
      // If favicon.ico occurs
      else if (command == "favicon.ico HTTP");
      // Else send index
      else {
        esp8266.flush();
        if (loadIndex(connectionId)) debug(F("Website send ok")); else debug(F("Website send error"));
      }
    }
  }
}

// Calculating angle from ADXL 335
void calcAngle() {

  int valX, valY, valZ;
  float accX, accY, accZ, angleX, angleY, angleZ;

  valX = analogRead(7);
  valY = analogRead(6);
  valZ = analogRead(5);

  accX = mapf(valX, 260.0, 392.0, +1.0, -1.0); //user defined mapf function
  accY = mapf(valY, 259.0, 391.0, +1.0, -1.0); //user defined mapf function
  accZ = mapf(valZ, 284.0, 418.0, -1.0, +1.0); //user defined mapf function

  //calculate the angle of inclination with each axis.
  angleX = atan2(accX, (sqrt(pow(accY, 2) + pow(accZ, 2)))) * (180 / PI);
  angleY = atan2(accY, (sqrt(pow(accX, 2) + pow(accZ, 2)))) * (180 / PI);
  angleZ = atan2((sqrt(pow(accX, 2) + pow(accY, 2))), accZ) * (180 / PI);

  //use fabs() "f"loating point absolute value vs abs()
  angle_x = fabs(angleX);
  angle_y = fabs(angleY);
  angle_z = fabs(angleZ);

}

// Mapping function
float mapf(float x_, float in_min, float in_max, float out_min, float out_max)
{
  return (x_ - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Send Thingspeak data
boolean sendTSData(int connection)
{
  green();
  int conn = 4;
  if (connection == 4)
    conn = 0;

  boolean success = true;

  String GET = F("GET /update?api_key=5HHQO96YD8H30SLU&field1=");
  GET += String(angle_x) + "&field2=" + String(angle_y) + "&field3=" + String(angle_z) + "\r\n\r\n";

  if (retryF(String(CIPSTART) + String(conn) + ",\"TCP\",\"" + Host + "\",80", OK)) {

    //if(sendCom(String(CIPSTART) + String(conn) + ",\"TCP\",\"" + Host + "\",80", OK)) {
    delay(20);
    if (retryF("AT+CIPSEND=4," + String(GET.length()), ">"))
    {
      delay(20);
      sendCom(GET);
    }
    else
      success = false;
    delay(20);
    success &= retryF(String(CIPCLOSE) + F("=4"), OK);
  }
  blue();
  return success;
}

// Read data from Thingspeak
boolean readTSData()
{
  boolean success = true;
  String GET = "GET /channels/391664/feeds.json?api_key=A28N9X93WWSAFO2W&results=1\r\n";
  if (sendCom(String(CIPSTART) + "4,\"TCP\",\"" + Host + "\",80", OK)) {
    delay(20);
    if (sendCom("AT+CIPSEND=4," + String(GET.length() + 2), ">"))
    {
      delay(20);
      sendCom(GET);
    }
    else
      success = false;

    if (esp8266.find("+IPD")) {
      if (esp8266.find("feeds")) {
        if (esp8266.find("\"field4\":\"")) {
          int dat = esp8266.parseInt();
          debug("Status at Google : " + dat);
        }
      }
    }
    delay(20);
    success &= sendCom(String(CIPCLOSE) + F("=4"), OK);
  }
  delay(20);

  return success;
}

// Function for rerequest in case of Error
boolean retryF(String command, char response[]) {

  int retry = 0;
  while (!sendCom(command, response)) {
    delay(20);
    if (retry++ > Mretry) {
      return false;
      break;
    }
  }
  return true;
}

// Close TCP connection function
void espClose(int connectionId, int maxretry) {
  retryF(String(CIPCLOSE) + "=" + String(connectionId), OK);
}

void red() {
  digitalWrite(4, HIGH);
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
}

void green() {
  digitalWrite(5, HIGH);
  digitalWrite(4, LOW);
  digitalWrite(6, LOW);
}

void blue() {
  digitalWrite(6, HIGH);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
}

//---------------------ESP Website send functions----------------------//

// Send website function providing Connection ID and String webpage
bool sendWebsite(int connectionId, String webpage)
{
  int retry = 0;
  bool success = true;

  esp8266.flush();
  delay(20);
  retryF("AT+CIPSEND=" + String(connectionId) + "," + String(webpage.length()), ">");
  delay(20);
  esp8266.print(webpage);
  delay(20);
  retry = 0;

  while (!esp8266.findUntil(SEND_OK, ERR)) {
    if (retry++ > Mretry) {
      success = false;  break;
    }
    delay(20);
  }
  return success;
}

// Index page function, because Website is bigger than 2kb
bool loadIndex(int connection) {

  bool success = true;
  int mLength = 0;
  String xBuffer;
  xBuffer = headerHTML;
  int retry = 0;

  unsigned int temp = 0;
  unsigned int test = headerHTML.length();

  if (headerHTML.length() + sizeof(homePage) > 500)
    mLength = 500;
  else
    mLength = headerHTML.length() + sizeof(homePage);

  while (test < (headerHTML.length() + sizeof(homePage)))
  {
    green();
    retry = 0;
    for (unsigned int i = 0; ((xBuffer.length() < mLength) && ((test) != (headerHTML.length() + sizeof(homePage)))) ; i++) {
      char myChar = pgm_read_byte_near(homePage + i + temp);
      xBuffer += myChar;
      test++;
    }
    esp8266.flush();
    retryF("AT+CIPSEND=" + String(connection) + "," + String(xBuffer.length()), ">");
    esp8266.print(xBuffer);
    delay(20);
    retry = 0;
    while (!esp8266.findUntil( "SEND OK", "ERROR")) {
      if (retry++ > Mretry) {
        success = false;  break;
      }
      delay(20);
    }
    xBuffer = "";
    temp = test - headerHTML.length();
  }
  espClose(connection, Mretry);
  blue();
  return success;
}

//-----------------------------------------Config ESP8266------------------------------------

// ESP configuration function
bool espConfig()
{
  bool success = true;

  esp8266.setTimeout(5000);
  success &= sendCom(F("AT"), OK);
  esp8266.setTimeout(5000);

  if (sendCom("AT+RST", "ready"));
  else  success = false;

  if (sendCom("AT+CWDHCP=1,1", OK));

  if (configStation(SSID, PASSWORD)) {
    success = true;
    debug("WLAN Connected");

    if (sendCom("AT+CIPSTA=\"192.168.137.307\"", "OK")) debug("Static IP 192.168.1.307"); else debug("Static ip error");
  }
  else
  {
    success &= false;
  }

  esp8266.setTimeout(1000);
  success &= sendCom(F("AT+CIPMODE=0"), OK);
  esp8266.setTimeout(1000);
  success &= sendCom(F("AT+CIPMUX=0"), OK);

  return success;
}

// Show IP
String getIP() {

  String IP;

  debug(F("My IP is:"));
  esp8266.setTimeout(1000);
  esp8266.println(F("AT+CIFSR"));

  if (esp8266.find("STAIP,\"")) {
    IP = esp8266.readStringUntil('"');
    debug(IP);
  }
  return IP;
}

// TCP server configuration
bool configTCPServer()
{
  bool success = true;

  success &= (sendCom(F("AT+CIPMUX=1"), OK));
  success &= (sendCom(F("AT+CIPSERVER=1,80"), OK));

  return success;
}

// Configuration for ESP as a Station
bool configStation(String vSSID, String vPASSWORT)
{
  bool success = true;
  success &= (sendCom(F("AT+CWMODE=1"), OK));
  esp8266.setTimeout(5000);
  while (!sendCom("AT+CWJAP=\"" + String(vSSID) + "\",\"" + String(vPASSWORT) + "\"", OK))
  {
    debug(F("retry !!"));
    delay(100);
  }
  return success;
}

//-----------------------------------------------Controll ESP-----------------------------------------------------
// Function for command responses
bool sendCom(String command, char respond[])
{
  esp8266.println(command);

  if ((esp8266.findUntil(respond, ERR)))
  {
    return true;
  }
  else
  {
    debug(F("ESP SEND ERROR: "));
    debug(command);
    return false;
  }
}

// Read response for a command
String sendCom(String command)
{
  Serial.flush();
  esp8266.flush();

  esp8266.println(command);
  return esp8266.readString();
}

//-------------------------------------------------Debug Functions------------------------------------------------------
// Serial debugging in case of error
void serialDebug() {
  while (true)
  {
    if (esp8266.available())
      Serial.write(esp8266.read());
    if (Serial.available())
      esp8266.write(Serial.read());
  }
}

// Terminal debugging
void debug(String Msg)
{
  if (DEBUG)
  {
    Serial.println(Msg);
  }
}
