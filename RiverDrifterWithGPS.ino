/** =========================================================================
 * RiverDrifterWithGPS.ino
 * Example of HydroSphere River Drifter with GPS and Timed Flotation Trigger
 *
 * author: Scott Ensign <ensign@stroudcenter.org>
 * copyright (c) 2020 Stroud Water Research Center (SWRC)
 *                          and the EnviroDIY Development Team
 *            This example is published under the BSD-3 license.
 *
 * Build Environment: Arduino IDE
 * Hardware Platform: EnviroDIY Mayfly Arduino Datalogger V0.3
 *
 * DISCLAIMER:
 * THIS CODE IS PROVIDED "AS IS" - NO WARRANTY IS GIVEN.
 * 
 * NOTE: Make sure EnviroDIY GPS bee's "ALWAYS ON" solder jumper is not shorted
 * ======================================================================= */
 
#include <TinyGPS.h>          // load this library for the TinyGPS
#include <Arduino.h>          // load this library for Arduino
#include <SPI.h>              // load this library for Serial Peripheral Interface
#include <SD.h>               // load this library to run the SD card
#include "Sodaq_DS3231.h"     // load this library to run the real time clock on the Mayfly Data Logger
 
DateTime triggerTime(2019,7,28,18,0,0,1);        //makes a "dateTime" object for YYYY,MM,DD,HH,MM,SS,last digit is for day of week: 1=Sun, 2=Mon, 3=Tue, 4=Wed, 5=Thu, 6=Fri, 7=Sat
uint32_t triggerSecond = triggerTime.getEpoch(); //converts to unix epoch in seconds
int solenoidOn = 3000;                           //milliseconds solenoid will be powered 
bool triggered = false;                          //set the initial condition that the solenoid has not been triggered
 
#define DATA_HEADER "sampleNumber, dateTime, batteryVolts, loggertempC, trigger, sats, HDOP, lat, long, age" // when using this code for just GPS (without timed trigger, remove "trigger,"
#define SD_SS_PIN 12                                                                                         // digital pin 12 is the MicroSD slave select pin on the Mayfly
#define FILE_NAME "datalog.txt"                                                                              // your name for the file to be created on the SD card
 
int long sampleinterval = 60000;          //time between GPS measurements, in milliseconds
int samplenum = 1;                        // declare the variable "samplenum" and start with 1
int batteryPin = A6;                      // on the Mayfly board, pin A6 is connected to a resistor divider on the battery input
int batterysenseValue = 0;                // variable to store the value coming from the analogRead function
float batteryvoltage;                     // the battery voltage as calculated by the formula below
const int TIME_ZONE = -5;                 // set the time zone to -4 for EDT or -5 for EST
static void smartdelay(unsigned long ms); 
 
String add02d(uint16_t val) {     // add a leading 0 to integers less than 10
  if (val < 10)                   //
    {return "0" + String(val);}   //
  else                            //
    {return String(val);}         //
}
 
TinyGPS gps;                   // name the TinyGPS library "gps"
 
void setup()                   // set up the hardware
{
  Serial.begin(9600);          // baud rate is 9600 for serial terminal window output
  rtc.begin();                 // start the real time clock 
  setupLogFile();              // initialize log file
  Serial.println(DATA_HEADER); // echo the data header to the serial connection for display in terminal window
  Serial1.begin(9600);         // baud rate is 9600 for the hardware serial port connected to the GPS on the Mayfly Bee header
  pinMode(23,OUTPUT);          // on the Mayfly Data Logger, pin 23 goes to the sleep pin of the Bee header (pin 9)
  digitalWrite(23,HIGH);       // set pin 23 to "on"
  pinMode(22, OUTPUT);         // on the Mayfly Data Logger, pin 22 provides switched power to the Grove ports, use either port D6-7 or D10-11 to power the gas solenoid valve
  digitalWrite(22, LOW);       // set pin 22 to "off" (low)
  pinMode(8,OUTPUT);           // on the Mayfly Data Logger, pin 8 provides power to a green LED light
  digitalWrite(8,LOW);         // set pin 8 to "off"
  pinMode(9,OUTPUT);           // on the Mayfly Data Logger, pin 9 provides power to a red LED light
  digitalWrite(9,LOW);         // set pin 9 to "off"
}
 
void loop(){                            // start the loop that will be running continously for this program
  String dataRec = createDataRecord();  // create a string to store the text of the data record
  logData(dataRec);                     // save the data record to the log file on the microSD card
  Serial.println(dataRec);              // echo the data to the serial connection
  smartdelay(1000);                     // pause 1000 milliseconds
  {
 DateTime now = rtc.now();                                              
 uint32_t timeNow = now.getEpoch();                                       // get the current time from the RTC
 if((timeNow + 3600*TIME_ZONE >=triggerSecond) && (triggered==false)){    // if the current time (adjusted by time zone) is after the trigger time and it hasn't yet triggered...
  digitalWrite(22,HIGH);                                                  // turn pin 22 "on"
  digitalWrite(8,LOW);                                                    // turn the green LED "off"
  digitalWrite(9,HIGH);                                                   // turn the red LED "on"
  uint32_t solenoidOff = millis() + solenoidOn;                           // create an integer called solenoidOff that equals the current time (in milliseconds) plus the length of time you specified that the solenoid should remain open
  while (millis() < solenoidOff) {}                                       // until solenoidOff time occurs...
  triggered = true;                                                       // change the triggered flag to true
  digitalWrite(22,LOW);                                                   // finally, turn "off" the power to the solenoid
  }
  if(triggered==true) {                                                    // if the trigger flag has previously been switched to true...
    digitalWrite(8,HIGH);                                                  // turn "on" the green LED
    digitalWrite(9,HIGH);                                                  // turn "on" the red LED
  }
  else {                                                                   // if the trigger time hasn't been reached...
  digitalWrite(8,HIGH);                                                    // keep the green LED "on"
  digitalWrite(9,LOW);                                                     // keep the red LED "off"
  }
}
delay(sampleinterval);                                                  // pause for the measurement interval you specified above
}
 
void setupLogFile() {
  if (!SD.begin(SD_SS_PIN)) {                                               // if the SD card hasn't yet started, initialise the SD card
    Serial.println("Error: SD card failed to initialise or is missing.");   // print this error message to the terminal window if the card is missing or corrupt
  }
  bool oldFile = SD.exists(FILE_NAME);                                      // check if the file already exists
  File logFile = SD.open(FILE_NAME, FILE_WRITE);                            // open the file in write mode
  if (!oldFile)                                                             // add header information if the file did not already exist
  {
    logFile.println(FILE_NAME);                                             // print the FILE_NAME to the terminal window
    logFile.println(DATA_HEADER);                                           // print the DATA_HEADER to the terminal window
  }
  logFile.close();                                                          // close the file to save it
}
 
void logData(String rec) {                            
  File logFile = SD.open(FILE_NAME, FILE_WRITE);      // re-open the file
  logFile.println(rec);                               // write the CSV data
  logFile.close();                                    // close the file to save it
}
 
String createDataRecord() {                                                 // create a function that writes a string of text data to the SD card 
  String data = "";                                                         // create a String to hold the data text
  data += samplenum;                                                        // populate the string called "data" with the sample number
  data += ",";                                                              // append a comma 
  DateTime now = rtc.makeDateTime(rtc.now().getEpoch()+TIME_ZONE*3600);     // get the current date and time
  data += now.year();                                                       // append the year 
  data += "-";                                                              // add a dash
  data += now.month();                                                      // append the month
  data += "-";                                                              // add a dash
  data += now.date();                                                       // append the day
  data += " ";                                                              // add a space
  data += add02d(now.hour());                                               // append the current hour, but using the function add02d will add a leading zero if less than 10
  data += ":";                                                              // add a colon
  data += add02d(now.minute());                                             // append the current minute, adding a leading zero if less than 10
  data += ",";                                                              // add a comma to seperate the data above from the next data
  batterysenseValue = analogRead(batteryPin);                               // reads the analog voltage on the batteryPin, reported in bits
  batteryvoltage = (3.3/1023) * 4.7 * batterysenseValue;                    // converts bits into volts (see batterytest sketch for more info)
  data += batteryvoltage;                                                   // appends the battery voltage to the data string
  data += ",";                                                              // add a comma to seperate the data above from the next data 
  rtc.convertTemperature();                                                 // run the function that gets temperature data from the real time clock
  data += rtc.getTemperature();                                             // append that temperature data to the string
  data += ",";                                                              // comma
  data += triggered;                                                        // write the status of the trigger flag (has the solenoid opened yet? 0=false, 1=true)
  data += ",";                                                              // comma
  data += gps.satellites();                                                 // write the number of gps satellites being tracked
  data += ",";                                                              // comma
  data += gps.hdop();                                                       // write the horizontal dilution of precision measured by the gps
  data += ",";                                                              // comma
  long lat,lon;                                                             // get the position from the gps
  unsigned long fix_age;                                                    // get the age of that lat long fix
  gps.get_position(&lat, &lon, &fix_age);                                   // turn the position information into a lat and long
  data += lat;                                                              // write the latitude
  data += ",";                                                              // comma
  data += lon;                                                              // write the longitude
  data += ",";                                                              // comma
  data += fix_age;                                                          // write how old the lat and long data are
  samplenum++;                                                              // increment the sample number          
  return data;                                                              // generate the text string with all of the above information
}
 
static void smartdelay(unsigned long ms) {    // part of the TinyGPS library
  unsigned long start = millis();             
  do   {                            
    while (Serial1.available())               
    gps.encode(Serial1.read());               
  } 
  while (millis() - start < ms);              
}
