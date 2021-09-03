//Set Variables for Moiture sensor
int moistValue = 0; //value for storing moisture value
int soilPin = 12;//Declare a variable for the soil moisture sensor

// Set Variables for RTC
#include "RTClib.h"
RTC_PCF8523 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Find and Inclued Files for SD
#include <SPI.h>
#include <SD.h>
#include "FS.h"

// Set Variables and Find and Inclued Files for EINK
#include "Adafruit_ThinkInk.h"
#define EPD_CS      15
#define EPD_DC      33
#define SRAM_CS     32
#define EPD_RESET   -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    -1 // can set to -1 to not use a pin (will wait a fixed delay)

// Wifi
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "wifiConfig.h"

String loginIndex, serverIndex;
WebServer server(80);

// 2.13" Monochrome displays with 250x122 pixels and SSD1675 chipset
ThinkInk_213_Mono_B72 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Set Variables and Find and Inclued Files Motor Shield
#include <Adafruit_MotorShield.h>

Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *myMotor = AFMS.getMotor(4);

void setup() {
  //Setup Serial Monitor to Monitor the Code Working
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }

  // Setup SD Card
  setupSD();

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  loadHTML();

  // Setup RTC
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  // The following line can be uncommented if the time needs to be reset.
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  rtc.start();

  // Setup EINK
  display.begin(THINKINK_MONO);
  display.clearBuffer();

  // Setup Motor
  AFMS.begin();
  myMotor->setSpeed(255);
  logEvent("System Initialisation...");

  // Setup Soil Monitor
  readSoil();


  // Webserver
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */

  server.on("/", HTTP_GET, []() {
    Serial.println("Index");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    Serial.println("serverIndex");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.begin();
}
}

void loop() {

  // Gets the current date and time, and writes it to the Eink display.
  String currentTime = getDateTimeAsString();

  drawText("The Current Time and\nMoisture Value is", EPD_BLACK, 2, 0, 0);

  // writes the current time on the bottom half of the display (y is height)
  drawText(currentTime, EPD_BLACK, 2, 0, 75);

  // Draws a line from the leftmost pixel, on line 50, to the rightmost pixel (250) on line 50.
  display.drawLine(0, 50, 250, 50, EPD_BLACK);

  int moisture = readSoil();
  drawText(String (moisture), EPD_BLACK, 2, 0, 100);
  display.display();

  waterPlant(moisture);

  // waits 180 seconds (3 minutes) as per guidelines from adafruit.
  delay(180000);
  display.clearBuffer();

  //State How to Draw the Text on the E-ink Monitor
  void drawText(String text, uint16_t color, int textSize, int x, int y) {
    display.setCursor(x, y);
    display.setTextColor(color);
    display.setTextSize(textSize);
    display.setTextWrap(true);
    display.print(text);

  }
  //State the Time as a String so the E-Ink Monitor can Display it
  String getDateTimeAsString() {
    DateTime now = rtc.now();

    //Prints the date and time to the Serial monitor for debugging.
    /*
      Serial.print(now.year(), DEC);
      Serial.print('/');
      Serial.print(now.month(), DEC);
      Serial.print('/');
      Serial.print(now.day(), DEC);
      Serial.print(" (");
      Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
      Serial.print(") ");
      Serial.print(now.hour(), DEC);
      Serial.print(':');
      Serial.print(now.minute(), DEC);
      Serial.print(':');
      Serial.print(now.second(), DEC);
      Serial.println();
    */

    // Converts the date and time into a human-readable format.
    char humanReadableDate[20];
    sprintf(humanReadableDate, "%02d:%02d:%02d %02d/%02d/%02d",  now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());

    return humanReadableDate;
  }

  //State What the Code Means When it Says "setupSD" in the Above Code
  void setupSD() {
    if (!SD.begin()) {
      Serial.println("Card Mount Failed");
      return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
      return;
    }
    Serial.println("SD Started");
  }

  //Find the Time and Date in the AdaLogger and Display to the Arduino
  void logEvent(String dataToLog) {
    /*
       Log entries to a file on an SD card.
    */
    // Get the updated/current time
    DateTime rightNow = rtc.now();

    // Open the log file
    File logFile = SD.open("/logEvents.csv", FILE_APPEND);
    if (!logFile) {
      Serial.print("Couldn't create log file");
      abort();
    }

    // Log the event with the date, time and data
    logFile.print(rightNow.year(), DEC);
    logFile.print(",");
    logFile.print(rightNow.month(), DEC);
    logFile.print(",");
    logFile.print(rightNow.day(), DEC);
    logFile.print(",");
    logFile.print(rightNow.hour(), DEC);
    logFile.print(",");
    logFile.print(rightNow.minute(), DEC);
    logFile.print(",");
    logFile.print(rightNow.second(), DEC);
    logFile.print(",");
    logFile.print(dataToLog);

    // End the line with a return character.
    logFile.println();
    logFile.close();
    Serial.print("Event Logged: ");
    Serial.print(rightNow.year(), DEC);
    Serial.print(",");
    Serial.print(rightNow.month(), DEC);
    Serial.print(",");
    Serial.print(rightNow.day(), DEC);
    Serial.print(",");
    Serial.print(rightNow.hour(), DEC);
    Serial.print(",");
    Serial.print(rightNow.minute(), DEC);
    Serial.print(",");
    Serial.print(rightNow.second(), DEC);
    Serial.print(",");
    Serial.println(dataToLog);
  }

  //Find the Moiture in the Soil Via the Soil Moisture Sensor and State to the Arduino
  int readSoil() {
    moistValue = analogRead(soilPin);//Read the SIG value form sensor
    return moistValue;//send current moisture value
  }

  void waterPlant(int moistureValue) {
    //The function is to be called waterPlant() which will take the
    //moisture value as an argument, and return no value.
  }

  String readFile(fs::FS & fs, const char * path) {
    Serial.printf("Reading file: %s\n", path);
    char c;
    String tempHTML = "";

    File file = fs.open(path);
    if (!file) {
      Serial.print("Failed to open file for reading: ");
      Serial.println(path);
      return "";
    }

    while (file.available()) {
      c = file.read();
      tempHTML += c;
    }
    file.close();
    return tempHTML;
  }



  void loadHTML() {
    serverIndex = readFile(SD, "/serverIndex.html");
    loginIndex = readFile(SD, "/loginIndex.html");
  }
