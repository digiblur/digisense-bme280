


/*
  digisense-bme280

  Requirements
  - Support for the ESP8266 boards. 
        - You can add it to the board manager by going to File -> Preference and pasting http://arduino.esp8266.com/stable/package_esp8266com_index.json
          into the Additional Board Managers URL field.
        - Download the ESP8266 dependencies by going to Tools -> Board -> Board Manager and searching for ESP8266 and installing it.
          (NOTE: version 2.3.0 is used as 2.4.0 and 2.4.1 have issues) 

  - Flash settings:
      - NodeMCU 1.0 (ESP-12E Module) or Wemos D1
      - CPU Freq 80mhz
      - Flash Size 4M (1M SPIFFS)
      - Uploads Speed 115200
  
  - You will also need to download the follow libraries by going to Sketch -> Include Libraries -> Manage Libraries
      - PubSubClient
      - ArduinoJSON
*/

#include <BME280I2C.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FS.h>

// Set configuration defaults for LED, pins, WiFi, MQTT, etc in the following file:  (if you pulled the Git, rename the sample_config.h to config.h to compile the code)
#include "config.h"

/**************************** PIN DEFINITIONS ********************************************/
#define luxPin    A0
#define intLED1Pin   BUILTIN_LED  // D0  Wemos D1
//#define intLED1Pin   LED_BUILTIN  // D0 NodeMCU
#define intLED1on  LOW
#define intLED1off HIGH

BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                  // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
BME280::PresUnit presUnit(BME280::PresUnit_inHg);
int period = 3000;
int lux_period = 15000;
unsigned long time_now = 0;
unsigned long time_now_2 = 0;

/**************************** SENSOR DEFINITIONS *******************************************/
//float luxValue;
int lux;
//float calcLux;
float diffLux = 30;

// difference in temperature to trigger a MQTT publish
float diffTemp = 0.5;
float tempValue;
float newTempValue;

float diffHum = 1.5;
float humValue;
float newHumValue;

float diffPres = 0.01;
float presValue;
float newPresValue;

float diffFeel = 0.2;
float feelValue;
float newFeelValue;

char ESP_Chip_ID[8];
char NodeID[16];

char LWT_top[40];
char temp_state_top[40];
char feel_state_top[40];
char humid_state_top[40];
char pres_state_top[40];
char lux_state_top[40];
char intLED1_state_top[40];
char intLED1_set_top[40];

//char message_buff[100];

//const int Buffer_Size = 300;

//#define MQTT_MAX_PACKET_SIZE 512

char node_hostname[25]     = def_hostname;
char wifi_ssid[25]    = "";
char wifi_pass[30]    = "";

char mqtt_server[25]  = "";
int  mqtt_port        = 0;
char mqtt_user[25]    = "";
char mqtt_pass[25]    = "";

char cfg_ver[5]       = "db01";
int  LED1option       = 1;

#define digicfg  "/digicfg.json"

WiFiClient espClient;
PubSubClient client(espClient);

/********************************** START SETUP*****************************************/
void setup() {

  Serial.begin(115200);
  while(!Serial) {} // Wait
  Wire.begin();
  
  pinMode(luxPin, INPUT);
  pinMode(intLED1Pin, OUTPUT); 
  digitalWrite(intLED1Pin, intLED1off);
  delay(10);

  while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }

  // initial support for SPIFFS based configuration file and WiFi Manager
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(digicfg)) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(digicfg, "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        configFile.close();
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(node_hostname, json["hostname"]);
          strcpy(wifi_ssid, json["wifi_ssid"]);
          strcpy(wifi_pass, json["wifi_pass"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          mqtt_port = json["mqtt_port"];
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          LED1option = json["led1option"];
          strcpy(cfg_ver, json["cfg_ver"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    } else {
      // /config not found
      Serial.println("No config found.  Formatting SPIFFS");
      SPIFFS.format();
      Serial.println("Creating new config file.");
      
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["hostname"] = node_hostname;
      json["wifi_ssid"] = wifi_ssid;
      json["wifi_pass"] = wifi_pass;
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
      json["mqtt_user"] = mqtt_user;
      json["mqtt_pass"] = mqtt_pass;
      json["led1option"] = LED1option;
      json["cfg_ver"] = cfg_ver;
      File configFile = SPIFFS.open(digicfg, "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
    //end save
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  sprintf(ESP_Chip_ID, "%06X", ESP.getChipId());
  sprintf(NodeID, def_hostname, ESP_Chip_ID);

// setup topics w/ NODEID 
  sprintf(LWT_top, LWT_topic, NodeID);
  sprintf(temp_state_top, temp_state_topic, NodeID);
  sprintf(feel_state_top, feel_state_topic, NodeID);
  sprintf(humid_state_top, humid_state_topic, NodeID);
  sprintf(pres_state_top, pres_state_topic, NodeID);
  sprintf(lux_state_top, lux_state_topic, NodeID);
  sprintf(intLED1_state_top, intLED1_state_topic, NodeID);
  sprintf(intLED1_set_top, intLED1_set_topic, NodeID);

// OTA Flash Sets
  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(NodeID);
  ArduinoOTA.setPassword((const char *)OTApassword);

  Serial.print("Starting Node: ");
  Serial.println(String(NodeID));

  setup_wifi();

  client.setServer(def_mqtt_server, def_mqtt_port);

// OTA Flash Start
  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  reconnect();
}

/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting ");
  Serial.print(NodeID);
  Serial.print(" to ");
  Serial.println(def_wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(NodeID);
  WiFi.begin(def_wifi_ssid, def_wifi_password);

  // toggle on board LED as WiFi comes up
  while (WiFi.status() != WL_CONNECTED) {
     digitalWrite(intLED1Pin, intLED1on);
     delay(70);
     digitalWrite(intLED1Pin, intLED1off);
     delay(45);
     Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ");
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  long rssi = WiFi.RSSI();
  Serial.print(" - RSSI: ");
  Serial.println(rssi);
}

bool sendpub(char* topic, char* mqmess, bool retain = true) {
    char myPubMsg[80];
    sprintf(myPubMsg,"%s = %s",topic,mqmess);
    Serial.println(myPubMsg);
    return client.publish(topic, mqmess, retain);
}

/********************************** START SEND STATE*****************************************/
void sendState(int topnum) {
  if (topnum == 1 || topnum == 2) {
//       float newfeelValue = dht.computeHeatIndex(tempValue, humValue, IsFahrenheit);
       float newfeelValue = calculateHeatIndex(humValue, tempValue);
       if (checkBoundSensor(newfeelValue, feelValue, diffFeel)) {
         feelValue = newfeelValue;
         char result2[5]; 
         dtostrf(feelValue, 3, 1, result2); 
         sendpub(feel_state_top,result2,true);
     }     
  }
  if (topnum == 1) {
     char result1[5]; 
     dtostrf(tempValue, 3, 1, result1); 
     sendpub(temp_state_top,result1,true);
  }
  if (topnum == 2) {
     char result2[5]; 
     dtostrf(humValue, 3, 1, result2); 
     sendpub(humid_state_top,result2,true);
  }  
  if (topnum == 3) {
     char result3[5]; 
     dtostrf(presValue, 3, 2, result3); 
     sendpub(pres_state_top,result3,true);
  }  
  if (topnum == 4) {
     char result4[16];
     itoa(lux, result4, 10);
     sendpub(lux_state_top,result4,true);
  }
}

/*
 * Calculate Heat Index value AKA "Real Feel"
 * NOAA heat index calculations taken from
 * http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
 */
float calculateHeatIndex(float humidity, float temp) {
  float heatIndex= 0;
  if (temp >= 80) {
    heatIndex = -42.379 + 2.04901523*temp + 10.14333127*humidity;
    heatIndex = heatIndex - .22475541*temp*humidity - .00683783*temp*temp;
    heatIndex = heatIndex - .05481717*humidity*humidity + .00122874*temp*temp*humidity;
    heatIndex = heatIndex + .00085282*temp*humidity*humidity - .00000199*temp*temp*humidity*humidity;
  } else {
     heatIndex = 0.5 * (temp + 61.0 + ((temp - 68.0)*1.2) + (humidity * 0.094));
  }

  if (humidity < 13 && 80 <= temp <= 112) {
     float adjustment = ((13-humidity)/4) * sqrt((17-abs(temp-95.))/17);
     heatIndex = heatIndex - adjustment;
  }

  return heatIndex;
}


/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(NodeID, def_mqtt_user, def_mqtt_password, LWT_top, 1,1,"Offline")) {
      client.publish(LWT_top,"Online", true);
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/********************************** START CHECK SENSOR **********************************/
bool checkBoundSensor(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

/********************************** START MAIN LOOP***************************************/
void loop() {

  ArduinoOTA.handle();
  
  if (!client.connected()) {
    // reconnect();
    software_Reset();
  }
  client.loop();

  if(millis() > time_now + period){
    time_now = millis();
    bme.read(newPresValue, newTempValue, newHumValue, tempUnit, presUnit);
    // check temp difference - do we need to update the status
    if (checkBoundSensor(newTempValue, tempValue, diffTemp)) {
      tempValue = newTempValue;
      sendState(1);
    }
    // check humidity difference - do we need to update the status
    if (checkBoundSensor(newHumValue, humValue, diffHum)) {
      humValue = newHumValue;
      sendState(2);
    }

    if (checkBoundSensor(newPresValue, presValue, diffPres)) {
      presValue = newPresValue;
      sendState(3);
    }
  }  
  if(millis() > time_now_2 + lux_period){
    time_now_2 = millis();

    // read the LUX sensor
    int newLUX = analogRead(luxPin);
    // check LUX difference - do we need to update the status
    if (checkBoundSensor(newLUX, lux, diffLux)) {
      lux = newLUX;
      sendState(4);
    }
  }  
}

/****reset***/
void software_Reset() // Restarts program from beginning but does not reset the peripherals and registers
{
Serial.print("ESP Reset...");
ESP.reset(); 
}
