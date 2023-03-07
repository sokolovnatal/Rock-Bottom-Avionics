/******************************************************************************************************/
/*** Title: Boom Boom Logic                                                                         ***/
/*** File: boom_boom_logic.ino                                                                      ***/
/*** Developers: Janos Banoczi-Ruof, insertOtherNamesHere                                           ***/
/*** Dates: Mar 2023 - May 2023                                                                     ***/
/*** Description: Logic for our rocket                                                              ***/
/******************************************************************************************************/

#include <boom_boom.h>      // Include this file in each file. All other files should be .cpp not .ino 
//                             Also put the functions you want to use in the .h file
#include <Adafruit_AHTX0.h>
#include <SPI.h>
#include <SD.h>

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only

  }