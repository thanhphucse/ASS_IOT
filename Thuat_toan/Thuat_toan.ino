#define SKETCH_NAME    "ESP-NOW GATEWAY"
#define SKETCH_VERSION "2022-01-01"
#define SKETCH_ABOUT   "ESP-Now Gateway template code and demonstrator of simultaneous ESP-Now and WiFi."
#define LED 15
/*
 * This is a template code for an ESP-Now Gateway for ESP8266 and ESP32. It assumes Arduino IDE.
 * 
 * Note. ESP8266 and ESP32 use different WiFi implementations why you must set the appropriate
 * #define USE_ESP8266 or USE_ESP32. Please remember to also set the appropriate BOARD in the 
 * ARDUINO IDE to find the correct include files.
 *
 * The Gateway receives messages on the Espressif proprietary protocol ESP-Now. It also provides a simple WEB server
 * displaying the received temperature data from all sensors. This is to demonstrate a single ESP8266 is capable of 
 * communicating on ESP-Now and 2.4 GHz Wifi simoultaneously. One restriction apply, this only works on WiFi channel 1 
 * and you must therefore set the router to "fixed" channel 1.
 * 
 * In order to perform the demonstration you will need: 
 * - 1x ESP8266 or ESP32 programmed with this sketch, acting as Gateway (to the WiFi/internet), and 
 * - 1-20x ESP8266 or ESP32 programmed with the espnow_sensor sketch and equipped with a temperature sensor 
 * (or anything else you connect and modify the code for).
 * 
 * This is a simple demo sketch but fully working. It demostrates simultaneous WiFi and ESP-Now on ESP8266 and ESP32.
 * You can add other Internet services like MQTT, Blynk, ThingSpeak etc. (I have been running this for 1.5+ years with 
 * those services.) 
 * 
 * AUTHOR:
 * Jonas Byström, https://github.com/jonasbystrom
 *
 * CREDITS:
 * Code related to ESP-Now is partly based on code from:
 *  - Anthony Elder @ https://github.com/HarringayMakerSpace/ESP-Now/blob/master/espnow-sensor-bme280/espnow-sensor-bme280.ino
 *  With important info from:
 *  - Erik Bakke @ https://www.bakke.online/index.php/2017/05/21/reducing-wifi-power-consumption-on-esp8266-part-2/
 *  And great info from:
 *  - ArduinoDIY @ https://arduinodiy.wordpress.com/2020/01/18/very-deepsleep-and-energy-saving-on-esp8266/
 */

//  HARDWARE
//  - WEMOS/LOLIN D1 Mini Pro V2.0.0  (but any ESP8266 board should work)
//  or
//  - WEMOS/LOLIN D32 Pro  (but any ESP32 board should work)
// 
//  HISTORY:
//  2021-12-30  First public version
//  2022-01-01  Support for ESP32 added. 
//

// ------------------------------------------------------------------------------------------
// ESP CONFIGS
// ------------------------------------------------------------------------------------------
//
// IMPORTANT. Select ESP8266 or ESP32. - Remember to also set the appropriate ESP8266 or ESP32 board in the ARDUINO IDE !
//#define USE_ESP8266                 // Select (uncomment) one of ESP8266 or ESP32. (And set BOARD in ARDUINO IDE.) 
#define USE_ESP32
      
// Different Wifi and ESP-Now implementations for ESP8266 and ESP32
#ifdef USE_ESP8266
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#endif

#ifdef USE_ESP32
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#endif

#define TOKEN "aXuFGfK2qIlMPlUXBPut"  

#define GPIO0_PIN 3

char thingsboardServer[] = "demo.thingsboard.io";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;

// We assume that all GPIOs are LOW
boolean SmartSW = false;
boolean LED_State;
String pub;

// ------------------------------------------------------------------------------------------
// ESP-NOW SYSTEM CONFIGS
// ------------------------------------------------------------------------------------------
//
// This is the MAC address to be installed (sensors shall then send to this MAC address)
uint8_t GatewayMac[] =      {0x02, 0x10, 0x11, 0x12, 0x13, 0x14};

    /*  Note, the Gateway listens on this MAC address. All sensors shall send to this MAC address.
     *  You can set any MAC address of your choice according to this table of "free-to-use local MAC addresses":
     *    {0x02, any, any, any, any, any}
     *    {0x06, any, ...}
     *    {0x0A, any, ...}
     *    {0x0E, any, ...}
     *  
     *  Further, it would be possible to use the built-in HW-MAC address of the ESP8266. But, in case you would need to  
     *  change the ESP (for any reasons) you would then need to update ALL sensors to the same new MAC address. 
     *  It is therefore better to use a soft MAC defined by code. This will be installed in any new ESP HW you may use.
     *  Just remeber to set a new MAC address for every new system (gateway+sensors) you install in paralell. *  
     */
 
// ESP-Now message format. Sensor data is transmitted using this struct.
typedef struct sensor_data_t {   
  int           ID;               // Unit no to identy which sensor is sending
  int        light;
  int        count;
  unsigned long updated;            // Epoch time when received by Gateway. Set by gateway/receiver. (Not used by sensor, but part of struct for convenience reasons.)
} sensor_data_t;


// ------------------------------------------------------------------------------------------
// ESP-NOW GATEWAY CONFIGS
// ------------------------------------------------------------------------------------------
// Router WiFi Credentials (runnning on 2.4GHz and Channel=1)
#define SOFTAP_SSID       "HoangTruong"
#define SOFTAP_PASS       "Hoangpro"

#define UNITS             1          // No of esp-now sensor units supported to receive from. ESP-Now has a maximum of 20


// ------------------------------------------------------------------------------------------
// GLOBALS
// ------------------------------------------------------------------------------------------
#ifdef USE_ESP8266
ESP8266WiFiMulti wifiMulti;
#endif
#ifdef USE_ESP32
WiFiMulti wifiMulti;
#endif

sensor_data_t bufSensorData;          // buffer for incoming data
sensor_data_t sensorData[UNITS+1];    // buffer for all sensor data


// ------------------------------------------------------------------------------------------
// ESP-NOW functions
// ------------------------------------------------------------------------------------------
//
// This is a callback function from ESP (?), anyway (!IMPORTANT)
//
//   (Note. I noted now, 1 1/2 years later, when i clean this up and retest a gateway with ESP32 - this initVariant
//   seems not to be called by ESP routines anymore. I have as a fix called this routine manually in the setup.
//   This could of course (?) be done also for ESP8266. But i dont want to touch that code since it has been working 
//   for 1.5 years now.)
//
void initVariant()
{
  #ifdef USE_ESP8266
  wifi_set_macaddr(SOFTAP_IF, &GatewayMac[0]);          //8266 code
  wifi_set_macaddr(STATION_IF, &GatewayMac[0]);         //8266 code
  #endif

  #ifdef USE_ESP32
  WiFi.mode(WIFI_AP); 
  esp_wifi_set_mac(ESP_IF_WIFI_AP, &GatewayMac[0]);     //ESP32 code
  #endif
}

#define AUTO 0
#define MANUAL 1
#define DAY 1
int light = 0;
int count = 0;
boolean SystemState = AUTO;
boolean LED_auto = LED_State;

void processData(){
  if (light != DAY){
    if (count <= 0){
        LED_auto = false;
    }
    else{
        LED_auto = true;
    }
  }
  else {
      LED_auto = false;
  }  
  switch (SystemState){
    case AUTO:
    {
      LED_State = LED_auto;
      if (SmartSW){
        LED_State = !LED_State;
        SystemState = MANUAL;
      }
      break;  
    }
    case MANUAL:
    {
      if (SmartSW){
          LED_State = !LED_State;
      }
      if (LED_State == LED_auto)
          SystemState = AUTO;
    }
    break;
  }
  pub = get_gpio_status();
  set_gpio_status();
  Serial.println("Sending current GPIO status ...");
  client.publish("v1/devices/me/attributes", pub.c_str());
  Serial.print("LED State: ");
  Serial.println(LED_State);
  Serial.print("System State: ");
  Serial.println(SystemState);
}


// Callback when data is received from any Sender
#ifdef USE_ESP8266
void OnDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len)
#endif
#ifdef USE_ESP32
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
#endif
{
  char macStr[24];
  snprintf(macStr, sizeof(macStr), " %02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("\nData received from: "); Serial.println(macStr);
  memcpy(&bufSensorData, data, sizeof(bufSensorData));

  // Print data
  Serial.print ("ID: ");
  Serial.print (bufSensorData.ID);
  Serial.println ("");
  Serial.print ("Light: ");
  Serial.println (bufSensorData.light);
  Serial.print ("Count: ");
  Serial.println (bufSensorData.count);
  Serial.print("Channel: ");
  Serial.println(WiFi.channel());
  if (bufSensorData.ID == 1){
    SmartSW = true;
    //LED_State = !LED_State;
    // Sending current GPIO status
    pub = get_gpio_status();
    set_gpio_status();
    Serial.println("Sending current GPIO status ...");
    client.publish("v1/devices/me/attributes", pub.c_str());
  }
  else if (bufSensorData.ID == 2){
    light = bufSensorData.light;
    count = bufSensorData.count;
  }
  processData();
  SmartSW = false;
  // Store data
  int i = bufSensorData.ID;
  if ( (i >= 1) && (i <= UNITS) ) {
    memcpy(&sensorData[i], data, sizeof(bufSensorData));
  };
}


// ------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------
void setup()
// ------------------------------------------------------------------------------------
{
  // Init Serial
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  while (!Serial) {};
  delay(100);                                     // Needed for some boards
  Serial.println("\n\n");
  
  // Print sketch intro ---------------------------
  Serial.println();
  Serial.println("===========================================");
  Serial.println(SKETCH_NAME);
  Serial.println(SKETCH_VERSION);
  Serial.println(SKETCH_ABOUT);
  Serial.println("===========================================");
  #ifdef USE_ESP32
  // initVariant() seems (?) not to be called anymore for ESP32. It used to ...? Anyway, just call it from here.
  // (And of course, we could maybe just move all the code here, also for ESP8266. Some other day ...) 
  initVariant();  
  #endif
  
  // Connect to WiFi ------------------------------
  Serial.print("Connecting to WiFi ");

  #ifdef USE_ESP8266
  WiFi.disconnect();                              // clear all configs
  WiFi.softAPdisconnect();
  #endif
  
  // Set device in AP mode to begin with
  WiFi.mode(WIFI_AP_STA);                         // AP _and_ STA is required (!IMPORTANT)

  wifiMulti.addAP(SOFTAP_SSID, SOFTAP_PASS);      // I use wifiMulti ... just by habit, i guess ....
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Come here - we are connected
  Serial.println(" Done");
 
  // Print WiFi data
  Serial.println("Set as AP_STA station.");
  Serial.print  ("SSID: "); Serial.println(WiFi.SSID());
  Serial.print  ("Channel: "); Serial.println(WiFi.channel());
  delay(1000);

  // Initialize ESP-Now ---------------------------

  // Config gateway AP - set SSID and channel 
  int channel = WiFi.channel();
  if (WiFi.softAP(SOFTAP_SSID, SOFTAP_PASS, channel, 1)) {
    Serial.println("AP Config Success. AP SSID: " + String(SOFTAP_SSID));
  } else {
    Serial.println("AP Config failed.");
  }
  
  // Print MAC addresses
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());

  // Init ESP-Now 
  #ifdef USE_ESP8266
  #define ESPNOW_SUCCESS 0
  #endif
  #ifdef USE_ESP32
  #define ESPNOW_SUCCESS ESP_OK
  #endif  

  if (esp_now_init() == ESPNOW_SUCCESS) {
    Serial.println("ESP - Now Init Success");
  } else {
    Serial.println("ESP - Now Init Failed");
    ESP.restart();                                // just restart if we cant init ESP-Now
  }
  client.setServer( thingsboardServer, 1883 );
  client.setCallback(on_message);
  // ESP-Now is now initialized. Register a callback fcn for when data is received
  esp_now_register_recv_cb(OnDataRecv);
}


// ------------------------------------------------------------------------------------
void loop()
// ------------------------------------------------------------------------------------
{
  if (WiFi.status() == WL_CONNECTED) {
    if ( !client.connected() ) {
      int channel = WiFi.channel();
      if (WiFi.softAP(SOFTAP_SSID, SOFTAP_PASS, channel, 1)) {
        Serial.println("Disconnect, change STA channel for ESP-NOW");
      } else {
        Serial.println("AP Config failed.");
      }
      reconnect();
    }
    client.loop();
  }
}

// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);
  if (methodName.equals("getGpioStatus")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } 
  else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
    SmartSW = true;
    LED_State = !LED_State;
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    pub = get_gpio_status();
    set_gpio_status();
    client.publish(responseTopic.c_str(), pub.c_str()); //Change GPIO panel
    client.publish("v1/devices/me/attributes", pub.c_str()); //Change GPIO status
  }
}

String get_gpio_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data[String(GPIO0_PIN)] = LED_State;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

void set_gpio_status() {
    // Output GPIOs state
    if(LED_State == true){
      Serial.println("A");
      digitalWrite(LED, LOW);
    }
    else{
      Serial.println("B");
      digitalWrite(LED, HIGH);
    }
    // Update GPIOs state
}

void reconnect() {
  #ifdef USE_ESP32
  // initVariant() seems (?) not to be called anymore for ESP32. It used to ...? Anyway, just call it from here.
  // (And of course, we could maybe just move all the code here, also for ESP8266. Some other day ...) 
  initVariant();  
  #endif
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.mode(WIFI_AP_STA);                         // AP _and_ STA is required (!IMPORTANT)
      wifiMulti.addAP(SOFTAP_SSID, SOFTAP_PASS);      // I use wifiMulti ... just by habit, i guess ....
      while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
    }
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      Serial.print("Channel: ");
      Serial.println(WiFi.channel());
      // Subscribing to receive RPC requests
      client.subscribe("v1/devices/me/rpc/request/+");
      // Sending current GPIO status
      Serial.println("Sending current GPIO status ...");
      client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
