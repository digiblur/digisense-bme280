
/************ WIFI and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
#define def_wifi_ssid "MyWiFi" 
#define def_wifi_password "Password"
#define def_mqtt_server "192.168.x.x"
#define def_mqtt_user "mqttUser" 
#define def_mqtt_password "mqttPass"
#define def_mqtt_port 1883
#define def_hostname "digisense-%s"  //  (ex: digisense-9C0A7D)

/************* MQTT TOPICS (change these topics as you wish)  **************************/
// Concatenated with the hostname (ex: "%s/temp/state" = digisense-9C0A7D/temp/state)
#define LWT_topic "%s/LWT"
#define temp_state_topic "%s/temp/state"
#define feel_state_topic "%s/feel/state"
#define humid_state_topic "%s/humid/state"
#define lux_state_topic "%s/lux/state"

#define intLED1_state_topic "%s/intLED1/state"
#define intLED1_set_topic "%s/intLED1/set"

const char* on_cmd = "ON";
const char* off_cmd = "OFF";

/**************************** SENSOR CONFIGURATION *****************************************/
#define IsFahrenheit true //to use Celsius change to false

/**************************** FOR OTA **************************************************/
int OTAport = 8266;
#define OTApassword "OTAPass" // change this to whatever password you want to use when you upload OTA

// Enables Serial and print statements
#define CONFIG_DEBUG false