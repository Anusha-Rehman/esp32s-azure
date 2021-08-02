/**
   A simple Azure IoT example for sending telemetry to Iot Hub.
*/

#include <WiFi.h>
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"
#include "config.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WebServer.h> //Local DNS Server used for redirecting all requests to the configuration portal (  https://github.com/zhouhan0126/DNSServer---esp32  )
#include <DNSServer.h> //Local WebServer used to serve the configuration portal (  https://github.com/zhouhan0126/DNSServer---esp32  )
#include <WiFiManager.h>   // WiFi Configuration Magic (  https://github.com/zhouhan0126/DNSServer---esp32  ) >>  https://github.com/zhouhan0126/DNSServer---esp32  (ORIGINAL)

StaticJsonDocument<800> doc;

char JSONMessage[500];
#include <SoftwareSerial.h>

#include <EEPROM.h>
#include <NTPClient.h>

SoftwareSerial RS485 (2, 4);  //RX, TX //Correct

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiManager wifiManager;


#define INTERVAL 10000
#define MESSAGE_MAX_LEN 256
// Please input the SSID and password of WiFi
const char* ssid     = "StormFiber-2.4G";
const char* password = "40842585";

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
//static const char* connectionString = "HostName=new-esp32s.azure-devices.net;DeviceId=new-esp32s;SharedAccessKey=jzhtdeToszKrmDJljzhbA6usLmYDw/JvFD24Trms2+Y=";
static const char* connectionString = "HostName=new-esp32s.azure-devices.net;DeviceId=esp32ss;SharedAccessKey=K71pPlQttYTpEtYIuXFspMjRag6n9Q+rdyAJ2QWDN5A=";

//const char *messageData = "{\"messageId\":%d, \"Temperature\":%f, \"Humidity\":%f}";
static bool hasIoTHub = false;
static bool hasWifi = false;
int messageCount = 1;
static bool messageSending = true;
static uint64_t send_interval_ms;

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    Serial.println("Send Confirmation Callback finished.");
  }
}

static void MessageCallback(const char* payLoad, int size)
{
  Serial.println("Message callback:");
  Serial.println(payLoad);
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  // Display Twin message.
  Serial.println(temp);
  free(temp);
}

static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature and humidity data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature and humidity data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}

void wifiConnect() {

  for (int i = 0; i < 30; i++)                      //Delay for 40 Sec in start
  {
    printf("Delay for WiFi %d sec\n", 30 - i);
    delay(1000);
  }
  wifiManager.setTimeout(120);                      //Timeout for hotspot

  //callback when entering AP configuration mode
  wifiManager.setAPCallback(configModeCallback);

  //callback for when you connect to a network
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //creates a network named ESP32 with password 12345678
  if (!wifiManager.autoConnect("ESP32", "12345678")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again
    ESP.restart();
    delay(5000);
  }
}

void wifiDelete() {
  //using this command, the settings are cleared from memory
  //if you have saved a network to connect automatically, it is deleted.
  wifiManager.resetSettings();
}

//callback indicating that ESP has entered AP mode
void configModeCallback (WiFiManager *myWiFiManager) {
  //  Serial.println("Entered config mode");
  Serial.println("Entered configuration mode");
  Serial.println(WiFi.softAPIP()); //prints the IP of the AP
  Serial.println(myWiFiManager->getConfigPortalSSID()); //prints the SSID created from the network </p> <p>} </p> <p> // callback that indicates that we have saved a new network to connect to (station mode)

}

void saveConfigCallback () {
  //  Serial.println("Should save config");
  Serial.println("Configuration saved");
  Serial.println(WiFi.softAPIP()); //prints the IP of the AP
}

void WiFiCheck()
{

  Serial.print("Connected to WiFi: ");
  Serial.println(wifiManager.getSSID()); //prints the SSID of the network to which it is connected
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 30; i++)
    {
      printf("Trying to connect with WiFi %d\n", 30 - i);
      delay(1000);
    }
    ESP.restart();
    delay(500);
  }
}

void forceHotspot() {
  //if the button was pressed
  if ( digitalRead(2) == HIGH) {
    Serial.println("reset"); // try to open the portal
    wifiManager.resetSettings();
    if (!wifiManager.startConfigPortal("ESP_AP", "12345678") ) {
      Serial.println("Failed to connect");
      delay(2000);
      ESP.restart();
      delay(1000);
    }
    Serial.println("Connected ESP_AP !!!");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("ESP32 Device");
  Serial.println("Initializing...");
  Serial.println(" > WiFi");
  Serial.println("Starting connecting WiFi.");
  wifiConnect();

    delay(10);
    WiFi.mode(WIFI_AP);
    WiFi.enableSTA(true);
  //
    WiFi.begin(ssid, password);
  //
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("Connecting to WiFi..");
    }
  //
    Serial.println("Connected to the WiFi network");
 
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(" > IoT Hub");

 
  Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
  Esp32MQTTClient_Init((const uint8_t*)connectionString, true);

  hasIoTHub = true;
  Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  Esp32MQTTClient_SetMessageCallback(MessageCallback);
  Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);
  Serial.println("Start sending events.");
  randomSeed(analogRead(0));
  send_interval_ms = millis();

}

void tempHumd()
{
  float temp = (float)random(0, 500) / 10;
  float hum = (float)random(0, 1000) / 10;
  //doc["timestamp"] = formattedTime;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  //doc["heatindex"] = "hi";
  JSON();
}

void JSON()
{
  // Start a new line
  Serial.println();
  serializeJsonPretty(doc, Serial);
  serializeJson(doc, JSONMessage); //Parse message
  doc.clear();
}
void Azuretojson()
{
  WiFiCheck();
  Esp32MQTTClient_Check();
}
void callingMain()
{

      EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(JSONMessage, MESSAGE);
      Esp32MQTTClient_SendEventInstance(message);
    
   
}
void loop() {

      tempHumd();
      callingMain();
      Azuretojson();   
  
  delay(10000);
}
