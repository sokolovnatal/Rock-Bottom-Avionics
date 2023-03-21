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

  //gloabls
String dataLabel1 = "";//fill in for column names as needed
String dataLabel2 = "";
bool label = true;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  

  if(SD.begin()){
    Serial.println("SD card is ready to use");    
  }else{
    Serial.println("SD card initialization failed");
  }

  avionicsFile = SD.create("");//Figure out how to create multiple files with different names, talk to avionics people

  if(avionicsFile){
    while(avionicsFile.avaliable()){
      Serial.write(avionicsFile.read());
    }
    avionicsFile.close();
  }else{
    Serial.println("error opening file");
  }    
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only

  }



void loop(){
	
    //print out column headers
    
    if (avionicsFile) {    
      while(label){ //runs once
        Serial.print(dataLabel1);
        Serial.print(" , ");//fill in label
        Serial.println(dataLabel2);
        label=false;        
      }
        avionicsFile.print();
        avionicsFile.print(",");    //print commas for the amount of columns
        avionicsFile.println();
        avionicsFile.close(); // close the file
  }else {
    Serial.println("error opening test.txt");
  }
  delay(3000);//stores data every __ seconds (currently 3)
}
  
//need to sort into functions ;-;
