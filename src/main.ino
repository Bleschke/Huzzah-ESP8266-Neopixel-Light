/* 
 * Brian Leschke
 * February 26, 2017
 * Adafruit Huzzah ESP 8266 Neopixel Light
 * Version 1.0
 * 
 * -- Changelog: -- 
 * 
 * 2/26/17 - Initial Release without weather events and holidays
 * 
 * 
*/

// includes
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif


// -------------------- CONFIGURATION --------------------

// ** mDNS and OTA Constants **
#define HOSTNAME "ESP8266-OTA-"            // Hostename. The setup function adds the Chip ID at the end.
#define PIN            0                   // Pin used for Neopixel communication
#define NUMPIXELS      1                   // Number of Neopixels connected to the Huzzah ESP8266

// ** Default WiFi connection information **
const char* ap_default_ssid = "esp8266";   // Default SSID.
const char* ap_default_psk  = "esp8266";   // Default PSK.

// ** FIRE-EMS INFORMATION **
char SERVER_NAME[]    = "WEB ADDRESS HERE"; // Address of the webserver
int SERVER_PORT             = PORT NUMBER HERE;       // webserver port

char Str[11];
int prevNum  = 0; //Number of previous emails before check
int num      = 0; //Number of emails after check

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Uncomment the next line for verbose output over UART.
#define SERIAL_VERBOSE

// ---------- OTA CONFIGURATION - DO NOT MODIFY ----------

bool loadConfig(String *ssid, String *pass)
{
  // open file for reading.
  File configFile = SPIFFS.open("/cl_conf.txt", "r");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt.");

    return false;
  }

  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  
  content.trim();

  // Check if ther is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
  if (pos == -1)
  {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1)
    {
      pos = content.indexOf("\r");
    }
  }

  // If there is no second line: Some information is missing.
  if (pos == -1)
  {
    Serial.println("Invalid content.");
    Serial.println(content);

    return false;
  }

  // Store SSID and PSK into string vars.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le);

  ssid->trim();
  pass->trim();

#ifdef SERIAL_VERBOSE
  Serial.println("----- file content -----");
  Serial.println(content);
  Serial.println("----- file content -----");
  Serial.println("ssid: " + *ssid);
  Serial.println("psk:  " + *pass);
#endif

  return true;
} // loadConfig

bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open("/cl_conf.txt", "w");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt for writing");

    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  configFile.close();
  
  return true;
} // saveConfig


void setup()
{
  pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF
  pixels.show(); // This sends the updated pixel color to the hardware.
  
  String station_ssid = "";
  String station_psk = "";

  Serial.begin(115200);
  pixels.begin(); // This initializes the NeoPixel library.
  
  delay(100);

  Serial.println("\r\n");
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  // Print hostname.
  Serial.println("Hostname: " + hostname);
  //Serial.println(WiFi.hostname());


  // Initialize file system.
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }

  // Load wifi connection information.
  if (! loadConfig(&station_ssid, &station_psk))
  {
    station_ssid = "";
    station_psk = "";

    Serial.println("No WiFi connection information available.");
  }

  // Check WiFi connection
  // ... check mode
  if (WiFi.getMode() != WIFI_STA)
  {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // ... Compare file config with sdk config.
  if (WiFi.SSID() != station_ssid || WiFi.psk() != station_psk)
  {
    Serial.println("WiFi config changed.");

    // ... Try to connect to WiFi station.
    WiFi.begin(station_ssid.c_str(), station_psk.c_str());

    // ... Pritn new SSID
    Serial.print("new SSID: ");
    Serial.println(WiFi.SSID());

    // ... Uncomment this for debugging output.
    //WiFi.printDiag(Serial);
  }
  else
  {
    // ... Begin with sdk config.
    WiFi.begin();
  }

  Serial.println("Wait for WiFi connection.");

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    Serial.write('.');
    //Serial.print(WiFi.status());
    delay(500);
  }
  Serial.println();

  // Check connection
  if(WiFi.status() == WL_CONNECTED)
  {
    // ... print IP Address
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Can not connect to WiFi station. Go into AP mode.");
    
    // Go into software AP mode.
    WiFi.mode(WIFI_AP);

    delay(10);

    WiFi.softAP(ap_default_ssid, ap_default_psk);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  // Start OTA server.
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
}

// ---------- OTA CONFIGURATION - DO NOT MODIFY ----------

// ---------- ESP 8266 FUNCTIONS - CAN BE REMOVED ----------

void FireEmsCheck() {
  WiFiClient client;
  if (client.connect(SERVER_NAME, SERVER_PORT)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.println("GET /GetGmail.php");  // Apache server pathway.
    client.println();
    int timer = millis();
    delay(2000);
  } 
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");  //cannot connect to server
  }

  // if there's data ready to be read:
  if (client.available()) {  
    int i = 0;   
    //put the data in the array:
    do {
      Str[i] = client.read();
      i++;
      delay(1);
    } while (client.available());
     
    // Pop on the null terminator:
    Str[i] = '\0';
    //convert server's repsonse to a int so we can evaluate it
    num = atoi(Str); 
     
    Serial.print("Server's response: ");
    Serial.println(num);
    Serial.print("Previous response: ");
    Serial.println(prevNum);
    if (prevNum < 0)
    { //the first time around, set the previous count to the current count
      prevNum = num; 
      Serial.println("First email count stored.");
    }
    if (prevNum > num)
    { // handle if count goes down for some reason
      prevNum = num; 
    }
  }
  else
    {
    Serial.println("No response from server."); //cannot connect to server.
    }
    Serial.println("Disconnecting."); //disconnecting from server to reconnect
    client.stop();
    
    // ---------------- FIRE\EMS: ALERT FOR FIRE\EMS CALL ----------------   
    
    if(num > prevNum) {
    Serial.println("FIRE/EMS ALERT!");  //alert for new email
    for(int x = 0; x < 200; x++)  // Neopixel LED blinks 60 times.
    {
      pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF  
      pixels.setPixelColor(0, pixels.Color(255,0,0)); // RED
      pixels.show(); // This sends the updated pixel color to the hardware.
      delay(150);
      pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF
      pixels.setPixelColor(0, pixels.Color(255,255,255)); // WHITE
      pixels.show(); // This sends the updated pixel color to the hardware.
      delay(150);
    } 
    prevNum = num;  //number of old emails =  number of new emails
  }
  else  //if email value is lower/equal to previous, no alert.
  {
    Serial.println("No New Alert Emails");
    pixels.setPixelColor(0, pixels.Color(0,0,0)); // OFF
    pixels.setPixelColor(0, pixels.Color(255,255,255)); // DEFAULT WHITE
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  pixels.setPixelColor(0, pixels.Color(255,255,255)); // DEFAULT WHITE
  pixels.show(); // This sends the updated pixel color to the hardware.
  delay(2000);
}

// ---------- ESP 8266 FUNCTIONS - CAN BE REMOVED ----------

void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  

  // ---------- USER CODE GOES HERE ----------

  // ** FireEMS Alert Check **
  FireEmsCheck();
  
  
  // ---------- USER CODE GOES HERE ----------
  yield();
}

