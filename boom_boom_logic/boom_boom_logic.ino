/******************************************************************************************************/
/*** Title: Boom Boom Logic                                                                         ***/
/*** File: boom_boom_logic.ino                                                                      ***/
/*** Developers: Janos Banoczi-Ruof, Natalia Sokolov                                                ***/
/*** Dates: Mar 2023 - April 2023                                                                   ***/
/*** Description: Logic for our rocket                                                              ***/
/******************************************************************************************************/
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM9DS1.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_LPS2X.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
/*#include <NativeEthernet.h>
#include <EthernetUdp.h>*/

#define AHT_ADDRESS 0x38

/* Note for the next time I (Janos) get back to coding this tomorrow. Should store data in a string as long as I havent filled 75% of the ram, then dump to SD 
   Make some superior I2C code */

File avionicsFile;
Adafruit_LSM9DS1 dof9 = Adafruit_LSM9DS1();
Adafruit_AHTX0 humidSensor;
Adafruit_LPS22 stressedSensor;

// globals
String DATALABEL1 = "Time";  // fill in for column names as needed
String DATALABEL2 = "";
bool addCSVHeaders = true;

String BASEFILENAME = "flightData_";

const int analogAccelXPin = 40;
const int analogAccelYPin = 39;
const int analogAccelZPin = 38;

// All set to zero so if by some horid reason, one doesn't work, it wont break any functions.
double ADXL_ACCEL_X = 0.0;  // Big accel
double ADXL_ACCEL_Y = 0.0;
double ADXL_ACCEL_Z = 0.0;
double LSM_ACCEL_X = 0.0;  // Small Accel
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
String data;


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
char filename[32];  // Has to be bigger than the length of the file name. 32 was arbitrarily chosen
void printDataViaSerial();
bool storeData();  // Appends timestamp, data type, and data to CSV file
void ADXLRead();   // Grabs data from the board
bool LSMInit();
void LSMRead();  // Grabs data from the board
bool AHTInit();
void AHTRead();
bool LPSInit();
void LPSRead();

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

  Wire.begin();
  //Wire.setClock(400000); // Default is 100kHz. Uncomment if needed
  /*AHT can go up to 400kHZ*/

  SDInit();          // Try to initialize the SD card. Stops the code and spits an error out if it can not
  fileNamePicker();  // Fine a name that we can use for the power session
  LSMInit();
  AHTInit();
  LPSInit();

  
  int former = millis();
  data = "";
  for (int i = 0; i < 120; i++) {
    LSMRead();   // Takes 3939 microseconds. is very complicated
    AHTRead();   // Used to take 42063 microseconds. Down to 2451us now. Can be "shortened" by splitng into AHTRequestNew() and AHTRead() and putting other taskes in between
    LPSRead();   // Takes 1077 microseconds
    ADXLRead();  // Takes 52 microseconds
    // Reading all takes 50206 microseconds
    // Takes about 200 microseconds
    data = data + "\r\n" + String(millis()) + "," + String(ADXL_ACCEL_X) + "," + String(ADXL_ACCEL_Y) + "," + String(ADXL_ACCEL_Z) + "," + String(LSM_ACCEL_X) + "," + String(LSM_ACCEL_Y) + "," + String(LSM_ACCEL_Z) + "," + String(LSM_GYRO_X) + "," + String(LSM_GYRO_Y) + "," + String(LSM_GYRO_Z) + "," + String(LSM_MAGNO_X) + "," + String(LSM_MAGNO_Y) + "," + String(LSM_MAGNO_Z) + "," + String(LSM_TEMP) + "," + String(AHT_TEMP) + "," + String(AHT_HUMID) + "," + String(LPS_PRESSURE) + "," + String(LPS_TEMP) + "," + String(MIC_RAW_DATA);
  }
  
  storeData();  // Takes 20000 microseconds. About 950ms per block. 80ms between each measurement
  int latter = millis();
  Serial.println(latter - former);
  Serial.println("Block written");
}

void loop() {
  // Im confuzzled what this code is meant to do. So I have ignored it ;-;
  // Print out column headers

  /*if (avionicsFile) {
    while (addCSVHeaders) {  //runs once
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
}





// functions

// Initializes the SD card
bool SDInit() {
  if (SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialized");
    return true;
  } else {
    Serial.println("SD card initialization failed.\nHalting code");
    return false;
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
bool storeData() {  // F*** Ardiuno and its stupid refusal to use 64 bit integers
  avionicsFile = SD.open(filename, FILE_WRITE);
  if (avionicsFile) {
    if (addCSVHeaders) {
      avionicsFile.println("TIMESTAMP,ADXL_ACCEL_X,ADXL_ACCEL_Y,ADXL_ACCEL_Z,LSM_ACCEL_X,LSM_ACCEL_Y,LSM_ACCEL_Z,LSM_GYRO_X,LSM_GYRO_Y,LSM_GYRO_Z,LSM_MAGNO_X,LSM_MAGNO_Y,LSM_MAGNO_Z,LSM_TEMP,AHT_TEMP,AHT_HUMID,LPS_PRESSURE,LPS_TEMP,MIC_RAW_DATA");
      addCSVHeaders = false;
    }
    avionicsFile.println(data);
    avionicsFile.close();
    return true;
  } else {
    return false;
  }
}

// Read the data from the ADXL377 (analog big accel)
void ADXLRead() {
  ADXL_ACCEL_X = analogRead(analogAccelXPin);
  ADXL_ACCEL_Y = analogRead(analogAccelYPin);
  ADXL_ACCEL_Z = analogRead(analogAccelZPin);
}

// Initiate the LSM sensor (9 dof one)
bool LSMInit() {
  if (!dof9.begin()) {
    return false;
  }
  dof9.setupAccel(dof9.LSM9DS1_ACCELRANGE_2G);    // Select accel range. ± 2, 4, 8, or 16 g
  dof9.setupMag(dof9.LSM9DS1_MAGGAIN_4GAUSS);     // Select gyro range.  ± 245, 500, and 2000 °/s
  dof9.setupGyro(dof9.LSM9DS1_GYROSCALE_245DPS);  // Select magno range. ± 4, 8, 12, or 16 gauss
  return true;
}

void LSMRead() {
  sensors_event_t a, m, g, temp;
  dof9.getEvent(&a, &m, &g, &temp);
  LSM_ACCEL_X = a.acceleration.x;
  LSM_ACCEL_Y = a.acceleration.y;
  LSM_ACCEL_Z = a.acceleration.z;
  LSM_GYRO_X = g.gyro.x;
  LSM_GYRO_Y = g.gyro.y;
  LSM_GYRO_Z = g.gyro.z;
  LSM_MAGNO_X = m.magnetic.x;
  LSM_MAGNO_Y = m.magnetic.y;
  LSM_MAGNO_Z = m.magnetic.z;
  LSM_TEMP = temp.temperature;
}

bool AHTInit() {
  // https://cdn-learn.adafruit.com/assets/assets/000/091/676/original/AHT20-datasheet-2020-4-16.pdf?1591047915
  while (millis() < 40) {  // boot up time is at max 20 ms
    delay(1);
  }
  // Check calibration enable bit of status word
  byte status;
  bool calibrated = false;
  while (!calibrated) {
    Wire.beginTransmission(AHT_ADDRESS);
    Wire.write(0x71);  // Status command byte
    Wire.endTransmission();
    Wire.requestFrom((int)AHT_ADDRESS, 1);
    status = Wire.read();
    if (status & (1 << 3)) {
      calibrated = true;
    } else {
      // Calibration enable bit is not set, send initialization command again
      Wire.beginTransmission(AHT_ADDRESS);
      Wire.write(0xBE);  // Command byte
      Wire.write(0x08);  // Initialization parameter byte 1
      Wire.write(0x00);  // Initialization parameter byte 2
      Wire.endTransmission();

      // Wait for 10ms
      delay(10);
    }
  }
  return true;
}

void AHTRead() {
  Wire.beginTransmission(AHT_ADDRESS);
  Wire.write(0xAC);  // Send measurement command
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();

  byte status = 0;
  int timeout_counter = 0;
  while ((status & 0x80) == 0x00) {  // Check if measurement is completed
    Wire.beginTransmission(AHT_ADDRESS);
    Wire.write(0x71);  // Send status command
    Wire.endTransmission(false);
    Wire.requestFrom((int)AHT_ADDRESS, 1);
    status = Wire.read();
    delay(1);  // Wait 1ms before checking again
    timeout_counter++;
    if (timeout_counter > 160) {  // Break out of loop after 100 iterations (160ms)
      break;
    }
  }

  byte buf[6];                       // Create a buffer to store the received data
  Wire.requestFrom(AHT_ADDRESS, 6);  // Request 6 bytes of data from the sensor
  for (int i = 0; i < 6; i++) {
    buf[i] = Wire.read();                                       // Read each byte of data and store it in the buffer
  }                                                             // First byte is the status
  AHT_HUMID = ((buf[1] << 16) | (buf[2] << 8) | buf[3]);  // Humidity from the next 2-4 bytes
  AHT_TEMP = ((buf[4] << 8) | buf[5]);                // Calculate temperature from the next 2 bytes
}

bool LPSInit() {
  if (!stressedSensor.begin_I2C()) {
    return false;
  } else {
    stressedSensor.setDataRate(LPS22_RATE_75_HZ);
    return true;
  }
}

void LPSRead() {
  sensors_event_t temp, pressure;
  stressedSensor.getEvent(&pressure, &temp);
  LPS_TEMP = temp.temperature;
  LPS_PRESSURE = pressure.pressure;
}

void printDataViaSerial() {
  Serial.println("TIMESTAMP,ADXL_ACCEL_X,ADXL_ACCEL_Y,ADXL_ACCEL_Z,LSM_ACCEL_X,LSM_ACCEL_Y,LSM_ACCEL_Z,LSM_GYRO_X,LSM_GYRO_Y,LSM_GYRO_Z,LSM_MAGNO_X,LSM_MAGNO_Y,LSM_MAGNO_Z,LSM_TEMP,AHT_TEMP,AHT_HUMID,LPS_PRESSURE,LPS_TEMP,MIC_RAW_DATA");
  Serial.println(String(millis()) + "," + String(ADXL_ACCEL_X) + "," + String(ADXL_ACCEL_Y) + "," + String(ADXL_ACCEL_Z) + "," + String(LSM_ACCEL_X) + "," + String(LSM_ACCEL_Y) + "," + String(LSM_ACCEL_Z) + "," + String(LSM_GYRO_X) + "," + String(LSM_GYRO_Y) + "," + String(LSM_GYRO_Z) + "," + String(LSM_MAGNO_X) + "," + String(LSM_MAGNO_Y) + "," + String(LSM_MAGNO_Z) + "," + String(LSM_TEMP) + "," + String(AHT_TEMP) + "," + String(AHT_HUMID) + "," + String(LPS_PRESSURE) + "," + String(LPS_TEMP) + "," + String(MIC_RAW_DATA));
}