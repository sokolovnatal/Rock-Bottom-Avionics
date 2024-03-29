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
Adafruit_LSM9DS1 dof9 = Adafruit_LSM9DS1();
Adafruit_LPS22 stressedSensor;

bool addCSVHeaders = true;
bool connectedToUmbilical = false;
bool ethernetStarted = true;

String BASEFILENAME = "flightData_";
String HTTP_req;
String command = "";

const int analogAccelXPin = 23;
const int analogAccelYPin = 22;
const int analogAccelZPin = 21;
const int analogBackupBatVPin = 17;
const int analogTeensyBatVPin = 16;
const int analogTrackerBatVPin = 20;
const int analogUmbilicalConnectedPin = 15;
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
bool ON_BAT_POWER = false;
bool BACKUP_ON = true;
String data;
String HTMLResponse = "";
//double umbilicalAnalogVal = 0.0; // FIXME: remove once umbilical threshold is found

// functions
void fileNamePicker();
char filename[32];  // Has to be bigger than the length of the file name. 32 was arbitrarily chosen
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
  pinMode(digitalActiveLowBatteryPowerControl, OUTPUT);
  pinMode(digitalMosfetControlPin, OUTPUT);
  digitalWrite(digitalActiveLowBatteryPowerControl, LOW);
  digitalWrite(digitalMosfetControlPin, LOW);

  checkUmbilical();

  HTMLResponse = "Ethernet link established, right before you launch, enable the data stream via <code>startdatastream</code>.";

  //connectedToUmbilical = true;  // FIXME: delete when done testing
  //Serial.begin(9600);           // Open serial communications and assume it is open. FIXME: delete when done testing

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

  // Ethernet Setup
  if (connectedToUmbilical) {
    Ethernet.begin(teensyMAC, teensyIP, teensyGateway, teensySubnet);  // Wee woo, and we have ethernet!
    ethernetStarted = true;
  }
}

void loop() {
  //connectedToUmbilical = true;  // FIXME: delete when done testing

  while (connectedToUmbilical) {
    if (ethernetStarted == false) {
      delay(1000);
      Ethernet.begin(teensyMAC, teensyIP, teensyGateway, teensySubnet);  // Wee woo, and we have ethernet!
      ethernetStarted = true;
    }
    

    readAll();  // Read all the sensors

    // Command handling
    if (command.toLowerCase() == "initsd" || command.toLowerCase() == "sd") {
      SD_CARD_WORKING = SD.begin(BUILTIN_SDCARD);
      if (SD_CARD_WORKING && (filename[0] == '\0')) {
        fileNamePicker();  // Find a name that we can use for the power session
        HTMLResponse = "SD Card Initialized. File name: " + String(filename);
      } else if (SD_CARD_WORKING && (filename[0] != '\0')) {
        HTMLResponse = "SD Card Initialized. Unable to assign a file name.";
      } else {
        HTMLResponse = "SD Card Initialization Failed.";
      }
      command = "";
    } else if (command.toLowerCase() == "startdatastream" || command.toLowerCase() == "sds") {
      command = "";
      FIRST_SAVE_AFTER_DC = true;
      digitalWrite(digitalActiveLowBatteryPowerControl, LOW);
      BACKUP_ON = true;
      command = "";
      if (ON_BAT_POWER) {
        HTMLResponse = "<strong>WEBSITE DISCONNECTED</strong>\\nStarted recording data at 100Hz.\\nYou are on battery power and are a go for launch.";
      } else {
        HTMLResponse = "<strong>WEBSITE DISCONNECTED</strong>\\nStarted recording data at 100Hz.\\nBattery power enabled, you are a go for launch.";
      }
      ON_BAT_POWER = true;
    } else if (command.toLowerCase() == "disablebatteries" || command.toLowerCase() == "db") {
      digitalWrite(digitalActiveLowBatteryPowerControl, HIGH);
      ON_BAT_POWER = false;
      command = "";
      HTMLResponse = "No longer drawing battery power. <strong>DO NOT LAUNCH.</strong>";
    } else if (command.toLowerCase() == "enablebatteries" || command.toLowerCase() == "eb") {
      digitalWrite(digitalActiveLowBatteryPowerControl, LOW);
      BACKUP_ON = true;
      command = "";
      ON_BAT_POWER = true;
      if (FIRST_SAVE_AFTER_DC) {
        HTMLResponse = "Drawing from onboard batteries. \\nYou are collecting data and are a go for launch.";
      } else {
        HTMLResponse = "Drawing from onboard batteries. \\n<strong>DO NOT LAUNCH. YOU ARE NOT COLLECTING DATA.</strong> Use <code>startdatastream</code> to fix.";
      }
    } /*else if (command.toLowerCase() == "disablebackupflight" || command.toLowerCase() == "dbf") {
      command = "";
      digitalWrite(digitalMosfetControlPin, HIGH);
      BACKUP_ON = false;
      HTMLResponse = "Backup flight computer powered down. \\n<strong>DO NOT LAUNCH. YOU WILL CATO WITH A BOOM.</strong> Use <code>enablebackupflight</code> to enable again.";
    } else if (command.toLowerCase() == "enablebackupflight" || command.toLowerCase() == "ebf") {
      command = "";
      digitalWrite(digitalMosfetControlPin, LOW);
      BACKUP_ON = true;
      if (!ON_BAT_POWER) {
        HTMLResponse = "Backup flight computer receiving power, kindly check for a beep. \\n<strong>DO NOT LAUNCH. YOU ARE NOT DRAWING FROM THE ONBOARD BATTERIES.</strong> Use <code>enablebatteries</code> to fix.";
      } else if (!FIRST_SAVE_AFTER_DC) {
        HTMLResponse = "Backup flight computer receiving power, kindly check for a beep. \\n<strong>DO NOT LAUNCH. YOU ARE NOT COLLECTING DATA.</strong> Use <code>startdatastream</code> to fix.";
      } else{
        HTMLResponse = "Backup flight computer receiving power, kindly check for a beep. \\nYou are a go for launch. You are recieving power and recording data.";
      }
    }*/ else if (command != ""){
      command = "";
      digitalWrite(digitalActiveLowBatteryPowerControl, LOW);
      BACKUP_ON = true;
      HTMLResponse = "Unknown command. Please try again. \\nOn the likely chance that things are going terrible wrong, I have switched to battery power and have turned on the backup flight computer if it was previously off.";
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
    //connectedToUmbilical = true;  // FIXME: delete when done testing

    if (FIRST_SAVE_AFTER_DC) {
      readChunkOfData();
      connectedToUmbilical = false;
    }
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
  } else if(!SD_CARD_WORKING) {
    SD_CARD_WORKING = SD.begin(BUILTIN_SDCARD);
    if (SD_CARD_WORKING && (filename[0] == '\0')) {
      fileNamePicker();
    }
    delay(1000);
  }

}





// functions
// Checks if the umbilical is connected
void checkUmbilical() {
  // umbilicalAnalogVal = analogRead(analogUmbilicalConnectedPin);
  if (analogRead(analogUmbilicalConnectedPin) > 725) {
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

void readChunkOfData() {
  writeToString(true);
  for (int i = 0; i < 60; i++) {  // FIXME: Figure out how many iterations we need
    AHTRequestNew();              // Request new set of data from AHT
    LSMRead();                    // Takes 3939 microseconds. is very complicated
    LPSRead();                    // Takes 1077 microseconds
    ADXLRead();                   // Takes 52 microseconds
    AHTRead();                    // Used to take 42063 microseconds. Down to 2451us now.

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

void batVRead() {
  BACKUP_BAT_V = analogRead(analogBackupBatVPin)/100.00;    // FIXME: Find exact linear scale
  TEENSY_BAT_V = analogRead(analogTeensyBatVPin)/100.00;    // FIXME: Find exact linear scale
  TRACKER_BAT_V = analogRead(analogTrackerBatVPin)/100.00;  // FIXME: Find exact linear scale
}

void writeToString(bool reset) {
  if (reset) {
    data = "";
  } else {
    data = data + "\r\n" + String(millis()) + "," + String(ADXL_ACCEL_X) + "," + String(ADXL_ACCEL_Y) + "," + String(ADXL_ACCEL_Z) + "," + String(LSM_ACCEL_X) + "," + String(LSM_ACCEL_Y) + "," + String(LSM_ACCEL_Z) + "," + String(LSM_GYRO_X) + "," + String(LSM_GYRO_Y) + "," + String(LSM_GYRO_Z) + "," + String(LSM_MAGNO_X) + "," + String(LSM_MAGNO_Y) + "," + String(LSM_MAGNO_Z) + "," + String(LSM_TEMP) + "," + String(AHT_TEMP) + "," + String(AHT_HUMID) + "," + String(LPS_PRESSURE) + "," + String(LPS_TEMP) + "," + String(MIC_RAW_DATA) + "," + String(BACKUP_BAT_V) + "," + String(TEENSY_BAT_V) + "," + String(TRACKER_BAT_V);
  }
}

void serveWebPage(EthernetClient client) {
  // turn into array of strings that is iterated through
  char *HTMLResponse[] = {
    "<!DOCTYPE html>",
    "  <head>",
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
    "    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=10\"/>",
    "    <title>Rock Bottom Control Panel</title>",
    "    <link rel=\"icon\" type=\"image/x-icon\" href=\"https://media.licdn.com/dms/image/C4E0BAQFfV1Z5fz81Pg/company-logo_200_200/0/1545441705816?e=2147483647&v=beta&t=QlYPMf0lc6jhELFH7kGZTiZUdky9Y3-F1CmqqvEGCZg\">",
    "    <script>",
    "      function updateSensorData() {",
    "        var xhttp = new XMLHttpRequest();",
    "        xhttp.onreadystatechange = function() {",
    "          if (this.readyState == 4 && this.status == 200) {",
    "            var data = JSON.parse(this.responseText);",
    "            document.getElementById('LSM_ACCEL_X').innerHTML = data.LSM_ACCEL_X;",
    "            document.getElementById('LSM_ACCEL_Y').innerHTML = data.LSM_ACCEL_Y;",
    "            document.getElementById('LSM_ACCEL_Z').innerHTML = data.LSM_ACCEL_Z;",
    "            document.getElementById('LSM_GYRO_X').innerHTML = data.LSM_GYRO_X;",
    "            document.getElementById('LSM_GYRO_Y').innerHTML = data.LSM_GYRO_Y;",
    "            document.getElementById('LSM_GYRO_Z').innerHTML = data.LSM_GYRO_Z;",
    "            document.getElementById('LSM_MAGNO_X').innerHTML = data.LSM_MAGNO_X;",
    "            document.getElementById('LSM_MAGNO_Y').innerHTML = data.LSM_MAGNO_Y;",
    "            document.getElementById('LSM_MAGNO_Z').innerHTML = data.LSM_MAGNO_Z;",
    "            document.getElementById('LSM_TEMP').innerHTML = data.LSM_TEMP;",
    "            document.getElementById('BACKUP_BAT_V').innerHTML = data.BACKUP_BAT_V;",
    "            document.getElementById('TEENSY_BAT_V').innerHTML = data.TEENSY_BAT_V;",
    "            document.getElementById('TRACKER_BAT_V').innerHTML = data.TRACKER_BAT_V;",
    "            document.getElementById('ADX_ACCEL_X').innerHTML = data.ADX_ACCEL_X;",
    "            document.getElementById('ADX_ACCEL_Y').innerHTML = data.ADX_ACCEL_Y;",
    "            document.getElementById('ADX_ACCEL_Z').innerHTML = data.ADX_ACCEL_Z;",
    "            document.getElementById('AHT_TEMP').innerHTML = data.AHT_TEMP;",
    "            document.getElementById('AHT_HUMID').innerHTML = data.AHT_HUMID;",
    "            document.getElementById('LPS_PRESSURE').innerHTML = data.LPS_PRESSURE;",
    "            document.getElementById('LPS_TEMP').innerHTML = data.LPS_TEMP;",
    "            if (data.SD_CARD_WORKING) { SDWorks = \"True\"; } else { SDWorks = \"False\"; };",
    "            document.getElementById('SD_CARD_WORKING').innerHTML = SDWorks;",
    "            document.getElementById('commandResponse').innerHTML = data.commandResponse;",
    "            if (data.SAVING_DATA_TO_BUFFER) { savingData = \"True\"; } else { savingData = \"False\"; };",
    "            document.getElementById('SAVING_DATA_TO_BUFFER').innerHTML = savingData;",
    "            if (data.ON_BAT_POWER) { batPwr = \"True\"; } else { batPwr = \"False\"; };",
    "            document.getElementById('ON_BAT_POWER').innerHTML = batPwr;",
    "            if (data.BACKUP_ON) { backupOn = \"True\"; } else { backupOn = \"False\"; };",
    "            document.getElementById('BACKUP_ON').innerHTML = backupOn;",
    "          }",
    "        };",
    "        xhttp.open('GET', '/ajax', true);",
    "        xhttp.send();",
    "      }",
    "      setInterval(updateSensorData, 1000);",
    "    </script>",
    "    <style>",
    "      body {",
    "        font-family: Arial, Helvetica, sans-serif;",
    "        font-size: 1rem;",
    "      }",
    "      table {",
    "        border-collapse: collapse;",
    "        border-spacing: 0;",
    "        margin-bottom: 1rem;",
    "      }",
    "      th, td {",
    "        text-align: left;",
    "        padding: 0.5rem;",
    "        border: 2px solid black;",
    "      }",
    "      ul.commandsList {",
    "        list-style-type: none;",
    "        margin: 0;",
    "        padding: 0;",
    "      }",
    "      ul.commandsList li {",
    "        padding: 0.5rem;",
    "      };",
    "      input[type=text] {",
    "        width: 25rem;",
    "        padding: 0.5rem;",
    "        margin: 0.5rem 0;",
    "        box-sizing: border-box;",
    "      }",
    "      input[type=submit] {",
    "        width: 10rem;",
    "        padding: 0.5rem;",
    "        margin: 0.5rem 0;",
    "        box-sizing: border-box;",
    "        background-color: white;",
    "      }",
    "      input[type=submit]:hover {",
    "        background-color: black;",
    "        color: white;",
    "      }",
    "      p#commandResponse {",
    "        margin: 0.5rem 0;",
    "      }",
    "      strong {",
    "        background-color: yellow;",
    "      }",
    "    </style>",
    "  </head>",
    "  <body>",
    "    <h1>Rock Bottom Control Panel</h1>",
    "    <form action=\"/\" method=\"get\">",
    "      <label for=\"command:\">Enter a command: </label><br>",
    "      <input type=\"text\" id=\"command\" name=\"command\" maxlength=\"50\">",
    "      <br>",
    "      <input type=\"submit\" name=\"submit\" value=\"Send commnd\">",
    "    </form>",
    "    <p id=\"commandResponse\">Command response</p>",
    "    <h4>Commands:</h4>",
    "    <ul class=\"commandsList\">",
    "      <li><code>initsd</code> - Try to initialize the SD card. Shorthand: <code>sd</code>.</li>",
    "      <li><code>startdatastream</code> - Start recording data to buffer. Slows site speed by a lot. Shorthand: <code>sds</code>.<strong>WILL DISCONNECT THE WEBSITE</strong></li>",
    "      <li><code>disablebatteries</code> - Stop drawing teensy power from batteries. Umbilical only. Shorthand: <code>db</code>.</li>",
    "      <li><code>enablebatteries</code> - Start drawing teensy power from batteries. Umbilical only. Shorthand: <code>eb</code>.</li>",
    "      <li><code>disablebackupflight</code> - Disable backup flight computer. Shorthand: <code>dbf</code>.</li>",
    "      <li><code>enablebackupflight</code> - Enable backup flight computer. Shorthand: <code>ebf</code>.</li>",
    "    </ul>",
    "    <table>",
    "      <tr>",
    "        <th>SAVING_DATA_TO_BUFFER</th>",
    "        <th>ON_BAT_POWER</th>",
    "        <th>BACKUP_ON</th>",
    "      </tr>",
    "      <tr>",
    "        <td id=\"SAVING_DATA_TO_BUFFER\">SAVING_DATA_TO_BUFFER</td>",
    "        <td id=\"ON_BAT_POWER\">ON_BAT_POWER</td>",
    "        <td id=\"BACKUP_ON\">BACKUP_ON</td>",
    "      </tr>",
    "    </table>",
    "    <h2>SD Card Working:</h2>",
    "    <p id=\"SD_CARD_WORKING\">SD_CARD_WORKING</p>",
    "    <h2>Battery Data:</h2>",
    "    <table>",
    "      <tr>",
    "        <th>BACKUP_BAT_V</th>",
    "        <th>TEENSY_BAT_V</th>",
    "        <th>TRACKER_BAT_V</th>",
    "      </tr>",
    "      <tr>",
    "        <td id=\"BACKUP_BAT_V\">BACKUP_BAT_V</td>",
    "        <td id=\"TEENSY_BAT_V\">TEENSY_BAT_V</td>",
    "        <td id=\"TRACKER_BAT_V\">TRACKER_BAT_V</td>",
    "      </tr>",
    "    </table>",
    "    <h2>LSM9DS1 Data:</h2>",
    "    <table>",
    "      <tr>",
    "        <th>LSM_ACCEL_X</th>",
    "        <th>LSM_ACCEL_Y</th>",
    "        <th>LSM_ACCEL_Z</th>",
    "        <th>LSM_GYRO_X</th>",
    "        <th>LSM_GYRO_Y</th>",
    "        <th>LSM_GYRO_Z</th>",
    "        <th>LSM_MAGNO_X</th>",
    "        <th>LSM_MAGNO_Y</th>",
    "        <th>LSM_MAGNO_Z</th>",
    "        <th>LSM_TEMP</th>",
    "      </tr>",
    "      <tr>",
    "        <td id=\"LSM_ACCEL_X\">LSM_ACCEL_X</td>",
    "        <td id=\"LSM_ACCEL_Y\">LSM_ACCEL_Y</td>",
    "        <td id=\"LSM_ACCEL_Z\">LSM_ACCEL_Z</td>",
    "        <td id=\"LSM_GYRO_X\">LSM_GYRO_X</td>",
    "        <td id=\"LSM_GYRO_Y\">LSM_GYRO_Y</td>",
    "        <td id=\"LSM_GYRO_Z\">LSM_GYRO_Z</td>",
    "        <td id=\"LSM_MAGNO_X\">LSM_MAGNO_X</td>",
    "        <td id=\"LSM_MAGNO_Y\">LSM_MAGNO_Y</td>",
    "        <td id=\"LSM_MAGNO_Z\">LSM_MAGNO_Z</td>",
    "        <td id=\"LSM_TEMP\">LSM_TEMP</td>",
    "      </tr>",
    "    </table>",
    "    <h2>ADXL377 Data:</h2>",
    "    <table>",
    "      <tr>",
    "        <th>ADX_ACCEL_X</th>",
    "        <th>ADX_ACCEL_Y</th>",
    "        <th>ADX_ACCEL_Z</th>",
    "      </tr>",
    "      <tr>",
    "        <td id=\"ADX_ACCEL_X\">ADX_ACCEL_X</td>",
    "        <td id=\"ADX_ACCEL_Y\">ADX_ACCEL_Y</td>",
    "        <td id=\"ADX_ACCEL_Z\">ADX_ACCEL_Z</td>",
    "      </tr>",
    "    </table>",
    "    <h2>AHT20 Data:</h2>",
    "    <table>",
    "      <tr>",
    "        <th>AHT_TEMP</th>",
    "        <th>AHT_HUMID</th>",
    "      </tr>",
    "      <tr>",
    "        <td id=\"AHT_TEMP\">AHT_TEMP</td>",
    "        <td id=\"AHT_HUMID\">AHT_HUMID</td>",
    "      </tr>",
    "    </table>",
    "    <h2>LPS22 Data:</h2>",
    "    <table>",
    "      <tr>",
    "        <th>LPS_TEMP</th>",
    "        <th>LPS_PRESSURE</th>",
    "      </tr>",
    "      <tr>",
    "        <td id=\"LPS_TEMP\">LPS_TEMP</td>",
    "        <td id=\"LPS_PRESSURE\">LPS_PRESSURE</td>",
    "      </tr>",
    "    </table>",
    "  </body>",
    "</html>"
  };
  // loop through array and print each line
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("Connection: close\r\n\r\n");

  for (int i = 0; i < sizeof(HTMLResponse); i++) {
    client.println(HTMLResponse[i]);
    //if(!server.available()) {return;} // FIXME: Uncomment and check if works and fixes umbilical disconnect issue
  };
  return;
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
  response += "\"HTMLResponse\": \"" + String(HTMLResponse) + "\",";
  response += "\"SAVING_DATA_TO_BUFFER\": " + String(FIRST_SAVE_AFTER_DC) + ",";
  response += "\"ON_BAT_POWER\": " + String(ON_BAT_POWER) + ",";
  response += "\"BACKUP_ON\": " + String(BACKUP_ON);
  response += "}";
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type:application/json\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("Connection: close\r\n\r\n");
  client.println(response);
  return;
}