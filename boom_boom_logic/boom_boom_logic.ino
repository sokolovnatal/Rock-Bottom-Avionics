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
#include <NativeEthernet.h>


// Setting up ethernet
// Set the static IP address to use if the DHCP fails to assign. | We want a static IP. DO NOT LET THE DHCP ASSIGN ONE. If we don't have a static IP, we have no simple way of finding the IP abd then connecting to it
byte teensyMAC[] = { 0xC0, 0xFF, 0xEE, 0xC0, 0xFF, 0xEE };  // Set's the Teensy's MAC address. Can be any random numbers that are not in use by another device on the same network. C0FFEE C0FFEE haha
byte teensyIP[] = { 192, 168, 137, 150 };                   // Teensy's id
byte teensyGateway[] = { 192, 168, 137, 1 };                // Unsure what this does, but we need it - J
byte teensySubnet[] = { 255, 255, 255, 0 };                 // Also unsure what this does, but we need it - J
EthernetServer server(80);                                  // Web server port

#define AHT_ADDRESS 0x38

File avionicsFile;
File calibrationFile;
Adafruit_LSM9DS1 dof9 = Adafruit_LSM9DS1();
Adafruit_LPS22 stressedSensor;

bool addCSVHeaders = true;
bool addCSVCalibrationHeaders = true;
bool connectedToUmbilical = true;

String BASEFILENAME = "flightData_";
String CALIBASEFILENAME = "CalibrationData_";
String HTTP_req;
String command = "";

const int analogAccelXPin = 23;
const int analogAccelYPin = 22;
const int analogAccelZPin = 21;
const int analogBackupBatVPin = 17;
const int analogTeensyBatVPin = 16;
const int analogTrackerBatVPin = 20;
const int digitalUmbilicalConnectedPin = 15;
const int digitalActiveLowBatteryPowerControl = 14;
const int digitalMosfetControlPin = 13;

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
bool SD_CARD_WORKING = false;
bool FIRST_SAVE_AFTER_DC = false;
bool ON_BAT_POWER = true;
String data;
String HTMLResponse = "";

// functions
void fileNamePicker();
char filename[32];             // Has to be bigger than the length of the file name. 32 was arbitrarily chosen
char CalibrationFilename[32];  // A new fresh name for the calibration data
void printDataViaSerial();
bool storeData();  // Appends most recent measurement to file
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
void serveWebPage(EthernetClient);
void serveAjaxRequest(EthernetClient);
void checkUmbilical();
void readAll();

void setup() {
  // Set up the pins
  pinMode(digitalUmbilicalConnectedPin, INPUT);
  pinMode(digitalActiveLowBatteryPowerControl, OUTPUT);
  pinMode(digitalMosfetControlPin, OUTPUT);
  digitalWrite(digitalActiveLowBatteryPowerControl, LOW);

  checkUmbilical();

  HTMLResponse = "Ethernet link established, right before you launch, enable the data stream via <code>startdatastream</code>.";

  connectedToUmbilical = true;  // FIXME: delete when done testing
  Serial.begin(9600);           // Open serial communications and assume it is open. FIXME: delete when done testing

  Wire.begin();           // Begin I2C
  Wire.setClock(400000);  // Default clock speed is 100kHz. Over vrooms it

  SD_CARD_WORKING = SD.begin(BUILTIN_SDCARD);      // Returns true if successful
  if (SD_CARD_WORKING && (filename[0] == '\0')) {  // If the SD card is working and we don't have a file name, then we need to find a file name
    fileNamePicker();                              // Find a name that we can use for the power session
  }

  // Initialize the sensors
  LSMInit();
  AHTInit();
  LPSInit();

  //calibrate(); // FIXME: finish calibrating stuff. Should be after ethernet is initialized

  // Ethernet Setup
  Ethernet.begin(teensyMAC, teensyIP, teensyGateway, teensySubnet);  // Wee woo, and we have ethernet!
}

void loop() {
  connectedToUmbilical = true;  // FIXME: delete when done testing

  while (connectedToUmbilical) {
    checkUmbilical();
    connectedToUmbilical = true;  // FIXME: delete when done testing

    readAll();  // Read all the sensors

    // Command handling
    if (command.toLowerCase() == "initsd" || command.toLowerCase() == "sd") {
      SD_CARD_WORKING = SD.begin(BUILTIN_SDCARD);
      if (SD_CARD_WORKING && (filename[0] == '\0')) {
        fileNamePicker();  // Find a name that we can use for the power session
        HTMLResponse = "SD Card Initialized. File name: " + String(filename);
      }else if (SD_CARD_WORKING && (filename[0] != '\0')) {
        HTMLResponse = "SD Card Initialized. Unable to assign a file name.";
      } else {
        HTMLResponse = "SD Card Initialization Failed.";
      }
      command = "";
    } else if (command.toLowerCase() == "startdatastream" || command.toLowerCase() == "sds") {
      command = "";
      FIRST_SAVE_AFTER_DC = true;
      if (ON_BAT_POWER){
        HTMLResponse = "Started recording data at 100Hz. Expect slow site response time. \\nYou are on battery power and are a go for launch.";
      } else {
        HTMLResponse = "Started recording data at 100Hz. Expect slow site response time. \\nDO NOT LAUNCH. YOU ARE NOT DRAWING FROM THE ONBOARD BATTERIES. Use <code>enablebatteries</code> to fix.";
      }
    } else if (command.toLowerCase() == "haltdatastream" || command.toLowerCase() == "hds") {
      command = "";
      FIRST_SAVE_AFTER_DC = false;
      HTMLResponse = "Stopped recording data. Site speed nominal again.";
    } else if (command.toLowerCase() == "disablebatteries" || command.toLowerCase() == "db") {
      digitalWrite(digitalActiveLowBatteryPowerControl, HIGH);
      ON_BAT_POWER = false;
      command = "";
      HTMLResponse = "No longer drawing battery power. Do not launch.";
    } else if (command.toLowerCase() == "enablebatteries" || command.toLowerCase() == "eb") {
      digitalWrite(digitalActiveLowBatteryPowerControl, LOW);
      command = "";
      if (FIRST_SAVE_AFTER_DC) {
        HTMLResponse = "Drawing from onboard batteries. \\nYou are collecting data and are a go for launch.";
      } else {
        HTMLResponse = "Drawing from onboard batteries. \\nDO NOT LAUNCH. YOU ARE NOT COLLECTING DATA. Use <code>startdatastream</code> to fix.";
      }
    } else if (command.toLowerCase() == "") {
    } else if (command.toLowerCase() == "") {
    }

    if (FIRST_SAVE_AFTER_DC) {
      readChunkOfData();
    }

    EthernetClient client = server.available();
    if (client) {
      String request = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          request += c;
          if (c == '\n') {
            if (request.indexOf("GET /ajax") != -1) {
              // AJAX request for sensor data
              serveAjaxRequest(client);
              break;
            } else {
              serveWebPage(client);

              if (request.indexOf("command") != -1) {
                int commandStartIndex = request.indexOf("command=") + 8;          // add 9 to skip "?command="
                int commandEndIndex = request.indexOf("&", commandStartIndex);    // find the next "&" after "?command="
                command = request.substring(commandStartIndex, commandEndIndex);  // extract the text between "?command=" and "&"
              }
              break;
            }
          }
        }
      }
      delay(1);
      client.stop();
    }
    checkUmbilical();
    connectedToUmbilical = true;  // FIXME: delete when done testing
  }

  // Immediately save data if we just disconnected from the umbilical. Hopefully captures liftoff data
  if (FIRST_SAVE_AFTER_DC) {
    storeData();
    FIRST_SAVE_AFTER_DC = false;
  }

  // If SD card is working:
  if (SD_CARD_WORKING) {
    readChunkOfData();
    storeData();
  }

  checkUmbilical();
}





// functions
// Checks if the umbilical is connected
void checkUmbilical() {
  if (digitalRead(digitalUmbilicalConnectedPin) == HIGH) {
    connectedToUmbilical = true;
  } else {
    connectedToUmbilical = false;
  }
  return;
}

// Read new values from all the sensors
void readAll() {
  AHTRequestNew();
  LSMRead();
  LPSRead();
  ADXLRead();
  AHTRead();
  batVRead();
  return;
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
  for (int i = 0; i < 60; i++) {  // FIXME: Figure out how many iterations we need
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
  LSM_ACCEL_X = a.acceleration.x;  // Takes raw data, multiplies by linear acceleration sensitivity, divides by 1000. is now Gs. then mulitplies by 9.80665
  LSM_ACCEL_Y = a.acceleration.y;
  LSM_ACCEL_Z = a.acceleration.z;
  LSM_GYRO_X = g.gyro.x;  // Takes raw data, multiplies by DPS, multiplies by 0.017453293 (deg. to rad multiplier)
  LSM_GYRO_Y = g.gyro.y;
  LSM_GYRO_Z = g.gyro.z;
  LSM_MAGNO_X = m.magnetic.x;  // Returns raw data???
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
  BACKUP_BAT_V = analogRead(analogBackupBatVPin);// 
  TEENSY_BAT_V = analogRead(analogTeensyBatVPin); // 
  TRACKER_BAT_V = analogRead(analogTrackerBatVPin); // 
}

void writeToString(bool reset) {
  if (reset) {
    data = "";
  } else {
    data = data + "\r\n" + String(millis()) + "," + String(ADXL_ACCEL_X) + "," + String(ADXL_ACCEL_Y) + "," + String(ADXL_ACCEL_Z) + "," + String(LSM_ACCEL_X) + "," + String(LSM_ACCEL_Y) + "," + String(LSM_ACCEL_Z) + "," + String(LSM_GYRO_X) + "," + String(LSM_GYRO_Y) + "," + String(LSM_GYRO_Z) + "," + String(LSM_MAGNO_X) + "," + String(LSM_MAGNO_Y) + "," + String(LSM_MAGNO_Z) + "," + String(LSM_TEMP) + "," + String(AHT_TEMP) + "," + String(AHT_HUMID) + "," + String(LPS_PRESSURE) + "," + String(LPS_TEMP) + "," + String(MIC_RAW_DATA) + "," + String(BACKUP_BAT_V) + "," + String(TEENSY_BAT_V) + "," + String(TRACKER_BAT_V);
  }
}

void serveWebPage(EthernetClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>Rock Bottom Control Panel</title>");
  client.println("<link rel=\"icon\" type=\"image/x-icon\" href=\"https://media.licdn.com/dms/image/C4E0BAQFfV1Z5fz81Pg/company-logo_200_200/0/1545441705816?e=2147483647&v=beta&t=QlYPMf0lc6jhELFH7kGZTiZUdky9Y3-F1CmqqvEGCZg\">");
  client.println("<script>");
  client.println("function updateSensorData() {");
  client.println("  var xhttp = new XMLHttpRequest();");
  client.println("  xhttp.onreadystatechange = function() {");
  client.println("    if (this.readyState == 4 && this.status == 200) {");
  client.println("      var data = JSON.parse(this.responseText);");
  client.println("      document.getElementById('LSM_ACCEL_X').innerHTML = data.LSM_ACCEL_X;");
  client.println("      document.getElementById('LSM_ACCEL_Y').innerHTML = data.LSM_ACCEL_Y;");
  client.println("      document.getElementById('LSM_ACCEL_Z').innerHTML = data.LSM_ACCEL_Z;");
  client.println("      document.getElementById('LSM_GYRO_X').innerHTML = data.LSM_GYRO_X;");
  client.println("      document.getElementById('LSM_GYRO_Y').innerHTML = data.LSM_GYRO_Y;");
  client.println("      document.getElementById('LSM_GYRO_Z').innerHTML = data.LSM_GYRO_Z;");
  client.println("      document.getElementById('LSM_MAGNO_X').innerHTML = data.LSM_MAGNO_X;");
  client.println("      document.getElementById('LSM_MAGNO_Y').innerHTML = data.LSM_MAGNO_Y;");
  client.println("      document.getElementById('LSM_MAGNO_Z').innerHTML = data.LSM_MAGNO_Z;");
  client.println("      document.getElementById('LSM_TEMP').innerHTML = data.LSM_TEMP;");
  client.println("      document.getElementById('BACKUP_BAT_V').innerHTML = data.BACKUP_BAT_V;");
  client.println("      document.getElementById('TEENSY_BAT_V').innerHTML = data.TEENSY_BAT_V;");
  client.println("      document.getElementById('TRACKER_BAT_V').innerHTML = data.TRACKER_BAT_V;");
  client.println("      document.getElementById('ADXL_ACCEL_X').innerHTML = data.ADXL_ACCEL_X;");
  client.println("      document.getElementById('ADXL_ACCEL_Y').innerHTML = data.ADXL_ACCEL_Y;");
  client.println("      document.getElementById('ADXL_ACCEL_Z').innerHTML = data.ADXL_ACCEL_Z;");
  client.println("      document.getElementById('AHT_TEMP').innerHTML = data.AHT_TEMP;");
  client.println("      document.getElementById('AHT_HUMID').innerHTML = data.AHT_HUMID;");
  client.println("      document.getElementById('LPS_PRESSURE').innerHTML = data.LPS_PRESSURE;");
  client.println("      document.getElementById('LPS_TEMP').innerHTML = data.LPS_TEMP;");
  client.println("      if (data.SD_CARD_WORKING) {SDWorks = \"True\";} else {SDWorks = \"False\";};");
  client.println("      document.getElementById('SD_CARD_WORKING').innerHTML = SDWorks;");
  client.println("      document.getElementById('commandResponse').innerHTML = data.HTMLResponse;");
  client.println("    }");
  client.println("  };");
  client.println("  xhttp.open('GET', '/ajax', true);");
  client.println("  xhttp.send();");
  client.println("}");
  client.println("setInterval(updateSensorData, 1000);");
  client.println("</script>");
  client.println("<style>");
  client.println("body {");
  client.println("  font-family: Arial, Helvetica, sans-serif;");
  client.println("  font-size: 1rem;");
  client.println("}");
  client.println("table {");
  client.println("  border-collapse: collapse;");
  client.println("  border-spacing: 0;");
  client.println("  margin-bottom: 1rem;");
  client.println("}");
  client.println("th {");
  client.println("  text-align: left;");
  client.println("  padding: 0.5rem;");
  client.println("  border: 2px solid black;");
  client.println("}");
  client.println("td {");
  client.println("  text-align: center;");
  client.println("  padding: 0.5rem;");
  client.println("  border: 2px solid black;");
  client.println("}");
  client.println("ul.commandsList {");
  client.println("  list-style-type: none;");
  client.println("  margin: 0;");
  client.println("  padding: 0;");
  client.println("}");
  client.println("ul.commandsList li {");
  client.println("  padding: 0.5rem;");
  client.println("}");
  
  client.println("input[type=text] {");
  client.println("  width: 25rem;");
  client.println("  padding: 0.5rem;");
  client.println("  margin: 0.5rem 0;");
  client.println("  box-sizing: border-box;");
  client.println("}");
  client.println("input[type=submit] {");
  client.println("  width: 10rem;");
  client.println("  padding: 0.5rem;");
  client.println("  margin: 0.5rem 0;");
  client.println("  box-sizing: border-box;");
  client.println("  background-color: white;");
  client.println("}");
  client.println("input[type=submit]:hover {");
  client.println("  background-color: black;");
  client.println("  color: white;");
  client.println("p#commandResponse {");
  client.println("  margin: 0.5rem 0;");
  client.println("}");

  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<h1>Rock Bottom Control Panel</h1>");

  client.println("<form action=\"/\" method=\"get\">");
  client.println("  <label for=\"command:\">Enter a command: </label><br>");
  client.println("  <input type=\"text\" id=\"command\" name=\"command\" maxlength=\"50\"><br>");
  client.println("  <input type=\"submit\" name=\"submit\" value=\"Send commnd\">");
  client.println("</form>");
  client.println("<p id=\"commandResponse\">Command response</p>");
  client.println("<h4>Commands:</h4>");
  client.println("<ul class=\"commandsList\">");
  client.println("  <li><code>initsd</code> - Try to initialize the SD card. Shorthand: <code>sd</code>.</li>");
  client.println("  <li><code>startdatastream</code> - Start recording data to buffer. Slows site speed by a lot. Shorthand: <code>sds</code>.</li>");
  client.println("  <li><code>haltdatastream</code> - Stop recording data to buffer. Speeds up site again. Shorthand: <code>hds</code>.</li>");
  client.println("  <li><code>disablebatteries</code> - Stop drawing teensy power from batteries. Umbilical only. Shorthand: <code>db</code>.</li>");
  client.println("  <li><code>enablebatteries</code> - Start drawing teensy power from batteries. Umbilical only. Shorthand: <code>eb</code>.</li>");
  client.println("</ul>");

  client.println("<h2>SD Card Working:</h2>");
  client.println("<p id=\"SD_CARD_WORKING\">SD_CARD_WORKING</p>");
  client.println("<h2>Battery Data:</h2>");
  client.println("<table>");
  client.println("  <tr>");
  client.println("    <th>BACKUP_BAT_V</th>");
  client.println("    <th>TEENSY_BAT_V</th>");
  client.println("    <th>TRACKER_BAT_V</th>");
  client.println("  </tr>");
  client.println("    <td id=\"BACKUP_BAT_V\">BACKUP_BAT_V</td>");
  client.println("    <td id=\"TEENSY_BAT_V\">TEENSY_BAT_V</td>");
  client.println("    <td id=\"TRACKER_BAT_V\">TRACKER_BAT_V</td>");
  client.println("  </tr>");
  client.println("</table>");
  client.println("<h2>LSM9DS1 Data:</h2>");
  client.println("<table>");
  client.println("  <tr>");
  client.println("    <th>LSM_ACCEL_X</th>");
  client.println("    <th>LSM_ACCEL_Y</th>");
  client.println("    <th>LSM_ACCEL_Z</th>");
  client.println("    <th>LSM_GYRO_X</th>");
  client.println("    <th>LSM_GYRO_Y</th>");
  client.println("    <th>LSM_GYRO_Z</th>");
  client.println("    <th>LSM_MAGNO_X</th>");
  client.println("    <th>LSM_MAGNO_Y</th>");
  client.println("    <th>LSM_MAGNO_Z</th>");
  client.println("    <th>LSM_TEMP</th>");
  client.println("  </tr>");
  client.println("  <tr>");
  client.println("    <td id=\"LSM_ACCEL_X\">LSM_ACCEL_X</td>");
  client.println("    <td id=\"LSM_ACCEL_Y\">LSM_ACCEL_Y</td>");
  client.println("    <td id=\"LSM_ACCEL_Z\">LSM_ACCEL_Z</td>");
  client.println("    <td id=\"LSM_GYRO_X\">LSM_GYRO_X</td>");
  client.println("    <td id=\"LSM_GYRO_Y\">LSM_GYRO_Y</td>");
  client.println("    <td id=\"LSM_GYRO_Z\">LSM_GYRO_Z</td>");
  client.println("    <td id=\"LSM_MAGNO_X\">LSM_MAGNO_X</td>");
  client.println("    <td id=\"LSM_MAGNO_Y\">LSM_MAGNO_Y</td>");
  client.println("    <td id=\"LSM_MAGNO_Z\">LSM_MAGNO_Z</td>");
  client.println("    <td id=\"LSM_TEMP\">LSM_TEMP</td>");
  client.println("  </tr>");
  client.println("</table>");
  client.println("<h2>ADXL377 Data:</h2>");
  client.println("<table>");
  client.println("  <tr>");
  client.println("    <th>ADXL_ACCEL_X</th>");
  client.println("    <th>ADXL_ACCEL_Y</th>");
  client.println("    <th>ADXL_ACCEL_Z</th>");
  client.println("  </tr>");
  client.println("  <tr>");
  client.println("    <td id=\"ADXL_ACCEL_X\">ADXL_ACCEL_X</td>");
  client.println("    <td id=\"ADXL_ACCEL_Y\">ADXL_ACCEL_Y</td>");
  client.println("    <td id=\"ADXL_ACCEL_Z\">ADXL_ACCEL_Z</td>");
  client.println("  </tr>");
  client.println("</table>");
  client.println("<h2>AHT20 Data:</h2>");
  client.println("<table>");
  client.println("  <tr>");
  client.println("    <th>AHT_TEMP</th>");
  client.println("    <th>AHT_HUMID</th>");
  client.println("  </tr>");
  client.println("  <tr>");
  client.println("    <td id=\"AHT_TEMP\">AHT_TEMP</td>");
  client.println("    <td id=\"AHT_HUMID\">AHT_HUMID</td>");
  client.println("  </tr>");
  client.println("</table>");
  client.println("<h2>LPS22 Data</h2>");
  client.println("<table>");
  client.println("  <tr>");
  client.println("    <th>LPS_TEMP</th>");
  client.println("    <th>LPS_PRESSURE</th>");
  client.println("  </tr>");
  client.println("  <tr>");
  client.println("    <td id=\"LPS_TEMP\">LPS_TEMP</td>");
  client.println("    <td id=\"LPS_PRESSURE\">LPS_PRESSURE</td>");
  client.println("  </tr>");
  client.println("</table>");
}

void serveAjaxRequest(EthernetClient client) {
  String response = "{";
  response += "\"LSM_ACCEL_X\": " + String(LSM_ACCEL_X) + ",";
  response += "\"LSM_ACCEL_Y\": " + String(LSM_ACCEL_Y) + ",";
  response += "\"LSM_ACCEL_Z\": " + String(LSM_ACCEL_Z) + ",";
  response += "\"LSM_GYRO_X\": " + String(LSM_GYRO_X) + ",";
  response += "\"LSM_GYRO_Y\": " + String(LSM_GYRO_Y) + ",";
  response += "\"LSM_GYRO_Z\": " + String(LSM_GYRO_Z) + ",";
  response += "\"LSM_MAGNO_X\": " + String(LSM_MAGNO_X) + ",";
  response += "\"LSM_MAGNO_Y\": " + String(LSM_MAGNO_Y) + ",";
  response += "\"LSM_MAGNO_Z\": " + String(LSM_MAGNO_Z) + ",";
  response += "\"LSM_TEMP\": " + String(LSM_TEMP) + ",";
  response += "\"BACKUP_BAT_V\": " + String(BACKUP_BAT_V) + ",";
  response += "\"TEENSY_BAT_V\": " + String(TEENSY_BAT_V) + ",";
  response += "\"TRACKER_BAT_V\": " + String(TRACKER_BAT_V) + ",";
  response += "\"ADXL_ACCEL_X\": " + String(ADXL_ACCEL_X) + ",";
  response += "\"ADXL_ACCEL_Y\": " + String(ADXL_ACCEL_Y) + ",";
  response += "\"ADXL_ACCEL_Z\": " + String(ADXL_ACCEL_Z) + ",";
  response += "\"AHT_TEMP\": " + String(AHT_TEMP) + ",";
  response += "\"AHT_HUMID\": " + String(AHT_HUMID) + ",";
  response += "\"LPS_PRESSURE\": " + String(LPS_PRESSURE) + ",";
  response += "\"LPS_TEMP\": " + String(LPS_TEMP) + ",";
  response += "\"SD_CARD_WORKING\": " + String(SD_CARD_WORKING) + ",";
  response += "\"HTMLResponse\": \"" + String(HTMLResponse) + "\"";
  response += "}";
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
  client.print(response);
}