/******************************************************************************************************/
/*** Title: Boom Boom Logic                                                                         ***/
/*** File: boom_boom_logic.ino                                                                      ***/
/*** Developers: Janos Banoczi-Ruof, Natalia Sokolov                                                ***/
/*** Dates: Mar 2023 - April 2023                                                                   ***/
/*** Description: Logic for our rocket                                                              ***/
/******************************************************************************************************/
#include <Adafruit_AHTX0.h>
#include <SPI.h>
#include <SD.h>

File avionicsFile;

// globals
String DATALABEL1 = "";  // fill in for column names as needed
String DATALABEL2 = "";
bool LABEL = true;

// functions
bool SDInit();

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    // Wait for serial port to connect. Needed for native USB port only
  }

  SDInit();

  avionicsFile = SD.open("test_1.txt", FILE_WRITE);  // Figure out how to create multiple files with different names, talk to avionics people
  if (avionicsFile) {
    Serial.println("File open.");
    avionicsFile.println("Ahemmm... Testing 1 2 3 testing");
    avionicsFile.close();
    Serial.println("File closed.");
  } else {
    Serial.println("Error opening file.");
  }


}



void loop() {


  // Im confuzzled what this code is meant to do. So I have ignored it ;-;
  // Print out column headers

  /*if (avionicsFile) {
    while (LABEL) {  //runs once
      Serial.print(DATALABEL1);
      Serial.print(" , ");  //fill in label
      Serial.println(DATALABEL2);
      LABEL = false;
    }
    avionicsFile.print();
    avionicsFile.print(",");  // Print commas for the amount of columns
    avionicsFile.println();
    avionicsFile.close();  // Close the file
  } else {
    Serial.println("error opening test.txt");
  }
  delay(3000);  //stores data every __ seconds (currently 3)*/
}





// functions

bool SDInit() {
  if (SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialized");
  } else {
    Serial.println("SD card initialization failed.\nHalting code");
    exit(0);
  }
}
