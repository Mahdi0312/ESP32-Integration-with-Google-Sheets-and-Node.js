#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>  // Include WiFiManager header
#include "time.h"
#include <ESP_Google_Sheet_Client.h>
#include <Wire.h>
#include <DFRobot_SHT3x.h>
#include "Adafruit_SGP30.h"

// For SD/SD_MMC mounting helper
#include <GS_SDHelper.h>

// Google Project ID
#define PROJECT_ID "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// Service Account's client email
#define CLIENT_EMAIL "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
// Service Account's private key
const char PRIVATE_KEY[] PROGMEM = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
// The ID of the spreadsheet where you'll publish the data
const char spreadsheetId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 8*30000;   // 30000 tick = 1/2 min

// Token Callback function
void tokenStatusCallback(TokenInfo info);

// Define the SDA and SCL pins for each I2C bus
const int SDA_PINS[] = {23, 22, 19, 18, 17, 5};    // only pin 15 is not usable
const int SCL_PINS[] = {21, 21, 21, 21, 21, 21};  // Assuming SCL is the same for each pair

const int NUM_SENSORS = 6;

TwoWire I2C_Wires[NUM_SENSORS] = {
  TwoWire(0),
  TwoWire(1),
  TwoWire(2),
  TwoWire(3),
  TwoWire(4),
  TwoWire(5)
};

DFRobot_SHT3x sht3x[NUM_SENSORS] = {
  DFRobot_SHT3x(&I2C_Wires[0], 0x44, 4),
  DFRobot_SHT3x(&I2C_Wires[1], 0x44, 4),
  DFRobot_SHT3x(&I2C_Wires[2], 0x44, 4),
  DFRobot_SHT3x(&I2C_Wires[3], 0x44, 4),
  DFRobot_SHT3x(&I2C_Wires[4], 0x44, 4),
  DFRobot_SHT3x(&I2C_Wires[5], 0x44, 4)
};

Adafruit_SGP30 sgp[NUM_SENSORS];

// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";

// AST is Atlantic Standard Time, ADT is Atlantic Daylight Time
const char* timeZone = "AST4ADT,M3.2.0,M11.1.0"; 


// Variable to save current epoch time
unsigned long epochTime;

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("N/A");
  }
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStr);
}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  // Set static IP address
  IPAddress local_IP(192,168,1,93);

  if (!WiFi.config(local_IP)) {
    Serial.println("STA Failed to configure");
  }

  // Configure time with the specified timezone
  configTime(0, 0, ntpServer);
  setenv("TZ", timeZone, 1);
  tzset();

  // Use WiFiManager to handle Wi-Fi connection
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32-Config"); // Create a config portal named "ESP32-Config"
  
  Serial.println("Connected to WiFi");

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Initialize sensors
  for (int i = 0; i < NUM_SENSORS; i++) {
    // Initialize I2C for each sensor with its custom SDA and SCL pins
    I2C_Wires[i].begin(SDA_PINS[i], SCL_PINS[i]);

    // Initialize SHT31
    if (sht3x[i].begin() != 0) {
      Serial.print("Failed to Initialize SHT31 chip on SDA pin ");
      Serial.println(SDA_PINS[i]);
    } else {
      Serial.print("SHT31 Chip serial number on SDA pin ");
      Serial.print(SDA_PINS[i]);
      Serial.print(": ");
      Serial.println(sht3x[i].readSerialNumber());

      if (!sht3x[i].softReset()) {
        Serial.print("Failed to Initialize the SHT31 chip on SDA pin ");
        Serial.println(SDA_PINS[i]);
      }

      if (!sht3x[i].heaterEnable()) {
        Serial.print("Failed to turn on the heater for SHT31 on SDA pin ");
        Serial.println(SDA_PINS[i]);
      }
    }

    // Initialize SGP30
    if (!sgp[i].begin(&I2C_Wires[i])) {
      Serial.print("SGP30 Sensor not found on SDA pin ");
      Serial.println(SDA_PINS[i]);
    } else {
      Serial.print("Found SGP30 serial # on SDA pin ");
      Serial.print(SDA_PINS[i]);
      Serial.print(": ");
      Serial.print(sgp[i].serialnumber[0], HEX);
      Serial.print(sgp[i].serialnumber[1], HEX);
      Serial.println(sgp[i].serialnumber[2], HEX);
    }

    // Delay to ensure I2C bus stability between initializations
    delay(100);
  }

  // Set the callback for Google API access token generation status (for debug only)
  GSheet.setTokenCallback(tokenStatusCallback);

  // Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
  GSheet.setPrerefreshSeconds(10 * 60);

  // Begin the access token generation for Google API authentication
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

  Serial.println("Setup complete.");
}

void loop() {
  // Call ready() repeatedly in loop for authentication checking and processing
  bool ready = GSheet.ready();

  if (ready && millis() - lastTime > timerDelay) {
    lastTime = millis();

    FirebaseJson response;

    Serial.println("\nAppend spreadsheet values...");
    Serial.println("----------------------------");

    FirebaseJson valueRange;
    FirebaseJsonArray valuesArray;

    String timestamp = getFormattedTime();

    for (int i = 0; i < NUM_SENSORS; i++) {
      // Reinitialize I2C bus before reading from sensors
      I2C_Wires[i].begin(SDA_PINS[i], SCL_PINS[i]);

      // Read temperature and humidity from SHT31
      DFRobot_SHT3x::sRHAndTemp_t data = sht3x[i].readTemperatureAndHumidity(sht3x[i].eRepeatability_High);
      String temperature = (data.ERR == 0) ? String(data.TemperatureC) : "Error";
      String humidity = (data.ERR == 0) ? String(data.Humidity) : "Error";

      // Read TVOC and eCO2 from SGP30
      String tvoc = (sgp[i].IAQmeasure()) ? String(sgp[i].TVOC) : "Error";
      String eco2 = (sgp[i].IAQmeasure()) ? String(sgp[i].eCO2) : "Error";
      String rawH2 = (sgp[i].IAQmeasureRaw()) ? String(sgp[i].rawH2) : "Error";
      String rawEthanol = (sgp[i].IAQmeasureRaw()) ? String(sgp[i].rawEthanol) : "Error";

      // Add sensor values to valuesArray
      FirebaseJsonArray rowData;
      rowData.add("Timestamp :");
      rowData.add(timestamp);
      rowData.add("SHT31");
      rowData.add("SDA PIN :");
      rowData.add(SDA_PINS[i]);
      rowData.add("Temperature (*C)");
      rowData.add(temperature);
      rowData.add("Humidity (%)");
      rowData.add(humidity);
            valuesArray.add(rowData);

      rowData.clear();
      rowData.add("Timestamp :");
      rowData.add(timestamp);
      rowData.add("SGP30");
      rowData.add("SDA PIN :");
      rowData.add(SDA_PINS[i]);
      rowData.add("TVOC (ppb)");
      rowData.add(tvoc);
      rowData.add("eCO2 (ppm)");
      rowData.add(eco2);
      rowData.add("Raw H2");
      rowData.add(rawH2);
      rowData.add("Raw Ethanol");
      rowData.add(rawEthanol);
      valuesArray.add(rowData);
    }

    valueRange.set("values", valuesArray);

    // For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/append
    // Append values to the spreadsheet
    bool success = GSheet.values.append(&response /* returned response */, spreadsheetId /* spreadsheet Id to append */, "Sheet1!A1" /* range to append */, &valueRange /* data range to append */);
    if (success) {
      response.toString(Serial, true);
      Serial.print("Free Heap: ");
      Serial.println(ESP.getFreeHeap());
    } else {
      Serial.println(GSheet.errorReason());
    }
    valueRange.clear();
    Serial.println();
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
  }
}

void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_error) {
    GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
  } else {
    GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
  }
}
