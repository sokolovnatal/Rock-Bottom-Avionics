/******************************************************************************************************/
/*** Title: Boom Boom Logic                                                                         ***/
/*** File: boom_boom_logic.ino                                                                      ***/
/*** Developers: Janos Banoczi-Ruof, Natalia Sokolov, Damian Suarez                                 ***/
/*** Dates: Mar 2023 - April 2023                                                                   ***/
/*** Description: Logic for our rocket                                                              ***/
/******************************************************************************************************/
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM9DS1.h>
#include <Adafruit_LPS2X.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
// Use to communicate with the FTP server
#include <NativeEthernet.h>


// Setting up ethernet
// Set the static IP address to use if the DHCP fails to assign. | We want a static IP. DO NOT LET THE DHCP ASSIGN ONE. If we don't have a static IP, we have no simple way of finding the IP abd then connecting to it
byte teensyMAC[] = { 0xC0, 0xFF, 0xEE, 0xC0, 0xFF, 0xEE };  // Set's the Teensy's MAC address. Can be any random numbers that are not in use by another device on the same network. C0FFEE C0FFEE haha
byte teensyIP[] = { 192, 168, 137, 150 };                   // last number of ip of laptop has to be different, get physical ethernet switch
byte teensyGateway[] = { 192, 168, 137, 1 };                // Unsure what this does, but we need it - J
byte teensySubnet[] = { 255, 255, 255, 0 };                 // Also unsure what this does, but we need it - J
EthernetServer server(80);                                  // Web server port

#define AHT_ADDRESS 0x38

File avionicsFile;
File calibrationFile;
Adafruit_LSM9DS1 dof9 = Adafruit_LSM9DS1();
Adafruit_LPS22 stressedSensor;

// globals
bool addCSVHeaders = true;
bool addCSVCalibrationHeaders = true;
bool connectedToUmbilical = true;

String BASEFILENAME = "flightData_";
String CALIBASEFILENAME = "CalibrationData_";
String HTTP_req;

const int analogAccelXPin = 40;
const int analogAccelYPin = 39;
const int analogAccelZPin = 38;
const int analogBackupBatVPin = 14;
const int analogTeensyBatVPin = 15;
const int analogTrackerBatVPin = 16;

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
double BACKUP_BAT_V = 0.0;
double TEENSY_BAT_V = 0.0;
double TRACKER_BAT_V = 0.0;
String data;

// functions
bool SDInit();
void fileNamePicker();
char filename[32];             // Has to be bigger than the length of the file name. 32 was arbitrarily chosen
char CalibrationFilename[32];  // A new fresh name for the calibration data
void printDataViaSerial();
bool storeData();  // Appends timestamp, data type, and data to CSV file
void ADXLRead();   // Grabs data from the board
bool storeData();  // Appends timestamp, data type, and data to CSV file
void ADXLRead();   // Grabs data from the board
bool LSMInit();
void LSMRead();  // Grabs data from the board
bool AHTInit();
void AHTRequestNew();
void AHTRead();
bool LPSInit();
void LPSRead();
void batVRead();
void writeToString(bool);
void readChunkOfData();

void setup() {
  Serial.begin(9600);  // Open serial communications and assume it is open. FIXME: delete when done testing

  Wire.begin();           // Begin I2C
  Wire.setClock(400000);  // Default clock speed is 100kHz. Over vrooms it

  SD.begin(BUILTIN_SDCARD);  // Returns true if successful
  fileNamePicker();          // Find a name that we can use for the power session

  // Initialize the sensors
  LSMInit();
  AHTInit();
  LPSInit();

  //calibrate(); // FIXME: finish calibrating stuff. Should be after ethernet is initialized

  // FIXME: Move this chonk of code to main with some logic as to when it should run
  readChunkOfData();

  storeData();  // Takes 20000 microseconds. About 950ms per block. 80ms between each measurement
  Serial.println("Block written");

  // Ethernet Setup
  Ethernet.begin(teensyMAC, teensyIP, teensyGateway, teensySubnet);  // Wee woo, and we have ethernet!

  // Print teensy's local IP address:
  Serial.print("My local address: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  while (connectedToUmbilical) {
    EthernetClient client = server.available();  // Create a client connection
    if (client)                                  // got client?
    {
      boolean currentLineIsBlank = true;
      while (client.connected()) {
        if (client.available())  // client data available to read
        {
          char c = client.read();  // read 1 byte (character) from client
          HTTP_req += c;           // save the HTTP request 1 char at a time
          if (c == '\n' && currentLineIsBlank) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();  // send web page
            client.println("<!DOCTYPE html>");
            client.println("<html>");
            client.println("<head>");
            client.println("<title>Rock Bottom Control Panel</title>");
            client.println("<link rel=\"icon\" type=\"image/x-icon\" href=\"https://media.licdn.com/dms/image/C4E0BAQFfV1Z5fz81Pg/company-logo_200_200/0/1545441705816?e=2147483647&v=beta&t=QlYPMf0lc6jhELFH7kGZTiZUdky9Y3-F1CmqqvEGCZg\">");
            client.println("</head>");
            client.println("<body>");
            client.println("<h1>Rock Bottom Control Panel</h1>");
            client.println("<p>OMG! Text is here!!!</p>");
            client.println("</body>");
            client.println("</html>");

            // Find the position of the second space character in the request string
            const char* start = strchr(HTTP_req.c_str(), ' ');
            if (start == NULL) {
              Serial.println("Invalid request");
              return;
            }
            start++;  // Move past the space character

            // Find the position of the next space character (after the resource path)
            const char* end = strchr(start, ' ');
            if (end == NULL) {
              Serial.println("Invalid request");
              return;
            }

            // Extract the resource path substring
            char resource[end - start + 1];
            strncpy(resource, start, end - start);
            resource[end - start] = '\0';

            Serial.print("Resource path: ");
            Serial.println(resource);

            HTTP_req = "";  // finished with request, empty string
            break;
          }

          if (c == '\n') {
            currentLineIsBlank = true;
          }

          else if (c != '\r') {
            currentLineIsBlank = false;
          }

        }  // end if (client.available())
      }    // end while (client.connected())

      delay(1);       // give the web browser time to receive the data
      client.stop();  // close the connection

    }  // end if (client)
  }
}





// functions

// Initializes the SD card
bool SDInit() {
  return SD.begin(BUILTIN_SDCARD);
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
      String calibFileNameStr = CALIBASEFILENAME + String(searchCount) + ".csv";
      calibFileNameStr.toCharArray(CalibrationFilename, calibFileNameStr.length() + 1);
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
      avionicsFile.println("TIMESTAMP,ADXL_ACCEL_X,ADXL_ACCEL_Y,ADXL_ACCEL_Z,LSM_ACCEL_X,LSM_ACCEL_Y,LSM_ACCEL_Z,LSM_GYRO_X,LSM_GYRO_Y,LSM_GYRO_Z,LSM_MAGNO_X,LSM_MAGNO_Y,LSM_MAGNO_Z,LSM_TEMP,AHT_TEMP,AHT_HUMID,LPS_PRESSURE,LPS_TEMP,MIC_RAW_DATA,BACKUP_BAT_V,TEENSY_BAT_V,TRACKER_BAT_V");
      addCSVHeaders = false;
    }
    avionicsFile.print(data);
    avionicsFile.close();
    return true;
  } else {
    return false;
  }
}

//Calblerate and exonerate (Create the Calibration File)
/*bool calibrate() { // I love arduino not using 64 bit integers it make my life so easy. F**k your love smh
  calibrationFile = SD.open(CalibrationFilename, FILE_WRITE);
  if (calibrationFile) {
    if (addCSVCalibrationHeaders) {
      calibrationFile.println("TIMESTAMP,ADXL_ACCEL_X,ADXL_ACCEL_Y,ADXL_ACCEL_Z,LSM_ACCEL_X,LSM_ACCEL_Y,LSM_ACCEL_Z,LSM_GYRO_X,LSM_GYRO_Y,LSM_GYRO_Z,LSM_MAGNO_X,LSM_MAGNO_Y,LSM_MAGNO_Z,LSM_TEMP,AHT_TEMP,AHT_HUMID,LPS_PRESSURE,LPS_TEMP,MIC_RAW_DATA,BACKUP_BAT_V,TEENSY_BAT_V,TRACKER_BAT_V");
      addCSVCalibrationHeaders = false;
    }
    //MAKE DO CALIBRATION
    Serial.println("Acceleration calibration initiated");
    Serial.println("Orient fore up, type \"prestodigitationingly\" and press enter: ");
    String entered;
    while((Serial.available() == 0) && (entered != "up")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_FORE_UP,9.8066");
    readChunkOfData();
    calibrationFile.print(data);
    Serial.println("Enough samples collected.");

    Serial.println("Flip stern up, type \"prestodigitationingly\" again and press enter: ");
    String entered;
    while((Serial.available() == 0) && (entered != "up")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_STERN_UP,9.8066");
    readChunkOfData();
    calibrationFile.print(data);
    Serial.println("Enough samples collected.");

    Serial.println("Orient face up, type \"prestodigitationingly\" again and press enter: ");
    String entered;
    while((Serial.available() == 0) && (entered != "up")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_FACE_UP,9.8066");
    readChunkOfData();
    calibrationFile.print(data);
    Serial.println("Enough samples collected.");

    Serial.println("Flip face down, type \"prestodigitationingly\" again and press enter: ");
    String entered;
    while((Serial.available() == 0) && (entered != "up")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_FACE_DOWN,9.8066");
    readChunkOfData();
    calibrationFile.print(data;)
    Serial.println("Enough samples collected.");

    Serial.println("Orient port up, type \"prestodigitationingly\" again and press enter: ");
    String entered;
    while((Serial.available() == 0) && (entered != "up")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_PORT_UP,9.8066");
    readChunkOfData();
    calibrationFile.print(data);
    Serial.println("Enough samples collected.");

    Serial.println("Flip starboard up, type \"prestodigitationingly\" again and press enter: ");
    String entered;
    while((Serial.available() == 0) && (entered != "up")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_STARBOARD_UP,9.8066");
    readChunkOfData();
    calibrationFile.print(data);
    Serial.println("Enough samples collected.");

    Serial.println("Accelerometer calibration collected.")
    Serial.println("Temperature calibration initiated");
    Serial.println("Type the current temperature, type it here and press enter: ");
    String entered = "";
    while((Serial.available() == 0) && (entered = "")) {
      entered = Serial.read();
    }
    calibrationFile.println("ACCEL_STARBOARD_UP," + String(entered));
    readChunkOfData();
    calibrationFile.print(data);
    Serial.println("Enough samples collected.");

    // add gyro, pressure, humity calibration code
    
    //calibrationFile.println(data);
    calibrationFile.close();
    return true;
  } else {
    return false;
  }
}*/

void readChunkOfData() {
  writeToString(true);
  for (int i = 0; i < 120; i++) {  // FIXME: Figure out how many iterations we need
    AHTRequestNew();               // Request new set of data from AHT
    LSMRead();                     // Takes 3939 microseconds. is very complicated
    LPSRead();                     // Takes 1077 microseconds
    ADXLRead();                    // Takes 52 microseconds
    AHTRead();                     // Used to take 42063 microseconds. Down to 2451us now.

    writeToString(false);  // Takes about 200 microseconds
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
  while (millis() < 40) {  // Boot up time is at max 20 ms
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

void AHTRequestNew() {
  Wire.beginTransmission(AHT_ADDRESS);
  Wire.write(0xAC);  // Send measurement command
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
}

void AHTRead() {
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
    buf[i] = Wire.read();                                 // Read each byte of data and store it in the buffer
  }                                                       // First byte is the status
  AHT_HUMID = ((buf[1] << 16) | (buf[2] << 8) | buf[3]);  // Humidity from the next 2-4 bytes
  AHT_TEMP = ((buf[4] << 8) | buf[5]);                    // Calculate temperature from the next 2 bytes
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
  Serial.println("TIMESTAMP,ADXL_ACCEL_X,ADXL_ACCEL_Y,ADXL_ACCEL_Z,LSM_ACCEL_X,LSM_ACCEL_Y,LSM_ACCEL_Z,LSM_GYRO_X,LSM_GYRO_Y,LSM_GYRO_Z,LSM_MAGNO_X,LSM_MAGNO_Y,LSM_MAGNO_Z,LSM_TEMP,AHT_TEMP,AHT_HUMID,LPS_PRESSURE,LPS_TEMP,MIC_RAW_DATA,BACKUP_BAT_V,TEENSY_BAT_V,TRACKER_BAT_V");
  Serial.println(String(millis()) + "," + String(ADXL_ACCEL_X) + "," + String(ADXL_ACCEL_Y) + "," + String(ADXL_ACCEL_Z) + "," + String(LSM_ACCEL_X) + "," + String(LSM_ACCEL_Y) + "," + String(LSM_ACCEL_Z) + "," + String(LSM_GYRO_X) + "," + String(LSM_GYRO_Y) + "," + String(LSM_GYRO_Z) + "," + String(LSM_MAGNO_X) + "," + String(LSM_MAGNO_Y) + "," + String(LSM_MAGNO_Z) + "," + String(LSM_TEMP) + "," + String(AHT_TEMP) + "," + String(AHT_HUMID) + "," + String(LPS_PRESSURE) + "," + String(LPS_TEMP) + "," + String(MIC_RAW_DATA));
}

void batVRead() {
  BACKUP_BAT_V = analogRead(analogBackupBatVPin);
  TEENSY_BAT_V = analogRead(analogTeensyBatVPin);
  TRACKER_BAT_V = analogRead(analogTrackerBatVPin);
}

void writeToString(bool reset) {
  if (reset) {
    data = "";
  } else {
    data = data + "\r\n" + String(millis()) + "," + String(ADXL_ACCEL_X) + "," + String(ADXL_ACCEL_Y) + "," + String(ADXL_ACCEL_Z) + "," + String(LSM_ACCEL_X) + "," + String(LSM_ACCEL_Y) + "," + String(LSM_ACCEL_Z) + "," + String(LSM_GYRO_X) + "," + String(LSM_GYRO_Y) + "," + String(LSM_GYRO_Z) + "," + String(LSM_MAGNO_X) + "," + String(LSM_MAGNO_Y) + "," + String(LSM_MAGNO_Z) + "," + String(LSM_TEMP) + "," + String(AHT_TEMP) + "," + String(AHT_HUMID) + "," + String(LPS_PRESSURE) + "," + String(LPS_TEMP) + "," + String(MIC_RAW_DATA) + "," + String(BACKUP_BAT_V) + "," + String(TEENSY_BAT_V) + "," + String(TRACKER_BAT_V);
  }
}
