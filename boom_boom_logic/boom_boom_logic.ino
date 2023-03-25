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
/*#include <NativeEthernet.h>
#include <EthernetUdp.h>*/

File avionicsFile;

// globals
String DATALABEL1 = "Time";  // fill in for column names as needed
String DATALABEL2 = "";
bool LABEL = true;

String BASEFILENAME = "flightData_";

const int analogAccelXPin = 40  ;
const int analogAccelYPin = 39;
const int analogAccelZPin = 38;

// All set to zero so if by some horid reason, one doesn't work, it wont break any functions.
double ADXL_ACCEL_X = 0.0;  // Big accel
double ADXL_ACCEL_Y = 0.0;
double ADXL_ACCEL_Z = 0.0;
double LSM_ACCEL_X = 0.0;   // Small Accel
double LSM_ACCEL_Y = 0.0;
double LSM_ACCEL_Z = 0.0;
double LSM_GYRO_X = 0.0;
double LSM_GYRO_Y = 0.0;
double LSM_GYRO_Z = 0.0;
double LSM_MAGNO_X = 0.0;
double LSM_MAGNO_Y = 0.0;
double LSM_MAGNO_Z = 0.0;
double LSM_TEMP = 0.0;
double AHT_TEMP = 0.0;
double AHT_HUMID = 0.0;
double LPS_PRESSURE = 0.0;
double LPS_TEMP = 0.0;
double MIC_RAW_DATA = 0.0;


/*const int pin = 10;  //Configuring ethernet pin

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;


// Enter a MAC address and IP address laptop below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED  //change to laptop
};
IPAddress ip(192, 168, 1, 177);            //assign static ip address
char serverName[] = "web.rockbottom.net";  //  test web page server

unsigned int localPort = 8888;  // local port to listen on*/

// functions
bool SDInit();
void fileNamePicker();
char filename[32];               // Has to be bigger than the length of the file name. 32 was arbitrarily chosen
bool storeData(String, double);  // Appends timestamp, data type, and data to CSV file
void measureAndStoreBigAccel();  // Grabs data from the board, and then passes it to the storeData function

void setup() {
  /*Ethernet.init(pin);
  Ethernet.begin(mac, ip);
  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1);  // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // start UDP
  Udp.begin(localPort);*/

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    // Wait for serial port to connect. Needed for native USB port only
  }

  SDInit();          // Try to initialize the SD card. Stops the code and spits an error out if it can not
  fileNamePicker();  // Fine a name that we can use for the power session
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
    Serial.println("error opening test.csv");
  }
  delay(3000);  //stores data every __ seconds (currently 3)*/
  measureAndStoreBigAccel();
  delay(10);
}





// functions

// Initializes the SD card
bool SDInit() {
  if (SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialized");
  } else {
    Serial.println("SD card initialization failed.\nHalting code");
    exit(0);
  }
}

// Returns the lowest numbered filename available
void fileNamePicker() {
  bool availableFileNumber = false;
  int searchCount = 0;
  String filenameStr;
  while (!availableFileNumber) {
    filenameStr = BASEFILENAME + String(searchCount) + ".csv";
    filenameStr.toCharArray(filename, filenameStr.length() + 1);
    if (!SD.exists(filename)) {
      availableFileNumber = true;
    }
    searchCount++;
  }
  return;
}

// Store data function. Takes timestamp, data type, and value. Returns true on success.
bool storeData(String dataType, double data) {  // F*** Ardiuno and its stupid refusal to use 64 bit integers
  avionicsFile = SD.open(filename, FILE_WRITE);
  if (avionicsFile) {
    avionicsFile.println(String(millis()) + "," + dataType + "," + String(data));
    avionicsFile.close();
    return true;
  } else {
    Serial.println("Failed to write file");
    return false;
  }
}

// Read and store the data from the ADXL377 (analog big accel)
void measureAndStoreBigAccel() {
  int rawX = analogRead(analogAccelXPin);
  int rawY = analogRead(analogAccelYPin);
  int rawZ = analogRead(analogAccelZPin);

  float scaledX = mapf(rawX, 0, 1023, -200, 200);
  float scaledY = mapf(rawY, 0, 1023, -200, 200);
  float scaledZ = mapf(rawZ, 0, 1023, -200, 200);

  Serial.println(String(scaledX));
  Serial.println(String(scaledY));
  Serial.println(String(scaledZ));

  storeData("BIGACCEL_X", scaledX);
  storeData("BIGACCEL_Y", scaledY);
  storeData("BIGACCEL_Z", scaledZ);
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
