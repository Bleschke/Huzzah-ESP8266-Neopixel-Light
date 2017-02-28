/* 
 * Brian Leschke
 * February 27, 2017
 * Adafruit Huzzah ESP 8266 Neopixel Light
 * The ESP8266 will control a neopixel and change the color based on Weather events, Holidays, and Fire/EMS calls.
 * Version 1.1
 * 
 *
 * -- Credit and Recognition: --
 * Morse Code Beacon Code by Mark VandeWettering (k6hx@arrl.net)
 *
 * -- Changelog: -- 
 * 
 * 2/26/17 - Initial Release - Fire/EMS and Weather Alerts implemented
 * 2/27/17 - Added NTP Client and Morse Code
 *
 *
 * 
*/

// includes
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif


// -------------------- CONFIGURATION --------------------

// ** mDNS and OTA Constants **
#define HOSTNAME "ESP8266-OTA-"     // Hostename. The setup function adds the Chip ID at the end.
#define PIN            0            // Pin used for Neopixel communication
#define NUMPIXELS      1            // Number of Neopixels connected to Arduino
// Neopixel Setup
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


// ** Default WiFi connection Information **
const char* ap_default_ssid = "esp8266";   // Default SSID.
const char* ap_default_psk  = "esp8266";   // Default PSK.


// ** Wunderground Information **
String responseString;
boolean startCapture;

const char   WxServer[]        = "api.wunderground.com"; // name address for Weather Underground (using DNS)
const String myKey             = "API-KEY";         //See: http://www.wunderground.com/weather/api/d/docs (change here with your KEY)
const String myWxAlertFeatures = "alerts";               // Do Not Change. See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
const String myWxFeatures      = "conditions";           // Do Not Change. See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
const String myState           = "ABBREV STATE";    //See: http://www.wunderground.com/weather/api/d/docs?d=resources/country-to-iso-matching
const String myCity            = "CITY";                 //See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1

long wxAlertCheckInterval           = 900000; // 15min default. Time (milliseconds) until next weather alert check
unsigned long previousWxAlertMillis = 0;
// long wxCheckInterval             = 900000; // 15min default. Time (milliseconds) until next weather check
// unsigned long previousWxMillis   = 0;


// ** FIRE-EMS INFORMATION **
char SERVER_NAME[]    = "SERVER ADDRESS"; // Address of the webserver
int SERVER_PORT       = PORT HERE;       // webserver port

char Str[11];
int prevNum           = 0; //Number of previous emails before check
int num               = 0; //Number of emails after check


// ** NTP SERVER INFORMATION **
// const char* timeHost = "time-c.nist.gov";
const char* timeHost    = "129.6.15.30";
const int timePort      = 13;

int ln = 0;
String TimeDate = "";

/*
WiFiUDP ntpUDP;
// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
NTPClient timeClient(ntpUDP); // default 'time.nist.gov'

*/

// ** MORSE CODE TRANSMISSION INFORMATION **
#define N_MORSE  (sizeof(morsetab)/sizeof(morsetab[0]))

#define SPEED  (12)
#define DOTLEN  (1200/SPEED)
#define DASHLEN  (3*(1200/SPEED))
int LEDpin = 2 ;

struct t_mtab { char c, pat; } ;

struct t_mtab morsetab[] = {
  {'.', 106},
  {',', 115},
  {'?', 76},
  {'/', 41},
  {'A', 6},
  {'B', 17},
  {'C', 21},
  {'D', 9},
  {'E', 2},
  {'F', 20},
  {'G', 11},
  {'H', 16},
  {'I', 4},
  {'J', 30},
  {'K', 13},
  {'L', 18},
  {'M', 7},
  {'N', 5},
  {'O', 15},
  {'P', 22},
  {'Q', 27},
  {'R', 10},
  {'S', 8},
  {'T', 3},
  {'U', 12},
  {'V', 24},
  {'W', 14},
  {'X', 25},
  {'Y', 29},
  {'Z', 19},
  {'1', 62},
  {'2', 60},
  {'3', 56},
  {'4', 48},
  {'5', 32},
  {'6', 33},
  {'7', 35},
  {'8', 39},
  {'9', 47},
  {'0', 63}
} ;


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
  
  String station_ssid = ""; // Do Not Change
  String station_psk = "";  // Do Not Change

  Serial.begin(115200);
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(LEDpin, OUTPUT) ; // Initialize Hellschreiber transmission output.
  
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
  //timeClient.begin(); // Start the time client

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
    Serial.println("Fire/EMS email check: connected");
    // Make a HTTP request:
    client.println("GET /GetGmail.php");  // Apache server pathway.
    client.println();
    int timer = millis();
    delay(2000);
  } 
  else {
    // if you didn't get a connection to the server:
    Serial.println("Fire/EMS email check: connection failed");  //cannot connect to server
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

void WeatherAlerts() {
  // if you get a connection, report back via serial:
  WiFiClient client;
  if (client.connect(WxServer, 80))
  {
    Serial.println("Connected to Wunderground!");
    
    String html_cmd1 = "GET /api/" + myKey + "/" + myWxAlertFeatures + "/q/" + myState + "/" + myCity + ".json HTTP/1.1";
    String html_cmd2 = "Host: " + (String)WxServer;
    String html_cmd3 = "Connection: close";
    
    //Uncomment this if necessary
    //Serial.println("Sending commands:");
    //Serial.println(" " + html_cmd1);
    //Serial.println(" " + html_cmd2);
    //Serial.println(" " + html_cmd3);
    //Serial.println();
    
    // Make a HTTP request:
    client.println(html_cmd1);
    client.println(html_cmd2);
    client.println(html_cmd3);
    client.println();
    
    responseString = "";
    startCapture = false;   
  } 
  else
  {
    // if you didn't get a connection to the server:
    Serial.println("Wunderground Weather Alerts: Connection failed.");
  }

  // if there are incoming bytes available 
  // from the server, read them and buffer:
  if (client.available())
  {
    char c = client.read();
    if(c == '{')
      startCapture=true;
    
    if(startCapture)
      responseString += c;
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {    
    Serial.println("Received " + (String)responseString.length() + " bytes");
    Serial.println("Disconnecting.");
    client.stop();
    client.flush();
    Serial.println();
    
    Serial.print("Alerts: ");
    if (responseString == "TOR") // Tornado Warning
    {
      Serial.println(getValuesFromKey(responseString, "alerts"));
      for(int x = 0; x < 200; x++)  // Neopixel LED blinks 200 times.
      {
        pixels.setPixelColor(0, pixels.Color(0,0,0));     // OFF  
        pixels.setPixelColor(0, pixels.Color(255,0,0));   // RED
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
        pixels.setPixelColor(0, pixels.Color(0,0,0));     // OFF
        pixels.setPixelColor(0, pixels.Color(255,165,0)); // ORANGE
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
      } 
    }
    else if (responseString == "TOW") // Tornado Watch
    {
      Serial.println(getValuesFromKey(responseString, "alerts"));
      for(int x = 0; x < 150; x++)  // Neopixel LED blinks 150 times.
      {
        pixels.setPixelColor(0, pixels.Color(0,0,0));     // OFF  
        pixels.setPixelColor(0, pixels.Color(255,0,0));   // RED
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
        pixels.setPixelColor(0, pixels.Color(0,0,0));     // OFF
        pixels.setPixelColor(0, pixels.Color(255,255,0)); // YELLOW
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
      } 
    }
    else if (responseString == "WRN") // Severe Thunderstorm Warning
    {
      Serial.println(getValuesFromKey(responseString, "alerts"));
      for(int x = 0; x < 150; x++)  // Neopixel LED blinks 150 times.
      {
        pixels.setPixelColor(0, pixels.Color(0,0,0));     // OFF
        pixels.setPixelColor(0, pixels.Color(255,165,0)); // ORANGE
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
        pixels.setPixelColor(0, pixels.Color(0,0,0));     // OFF
        pixels.setPixelColor(0, pixels.Color(255,255,0)); // YELLOW
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
      } 
    }    
    else if (responseString == "WIN") // Winter Weather
    {
      Serial.println(getValuesFromKey(responseString, "alerts"));
      for(int x = 0; x < 150; x++)  // Neopixel LED blinks 150 times.
      {
        pixels.setPixelColor(0, pixels.Color(0,0,0));       // OFF  
        pixels.setPixelColor(0, pixels.Color(255,153,171)); // Pink
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
        pixels.setPixelColor(0, pixels.Color(0,0,0));       // OFF
        pixels.setPixelColor(0, pixels.Color(0,249,255));   // Light Blue
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(250);
      } 
    }
    else {
      pixels.setPixelColor(0, pixels.Color(255,255,255)); // DEFAULT WHITE
      pixels.show(); // This sends the updated pixel color to the hardware.
      Serial.println("No Reportable Weather Alerts");
    }
  }
  
}

String getValuesFromKey(const String response, const String sKey)
{ 
  String sKey_ = sKey;
  
  sKey_ = "\"" + sKey + "\":";
  
  char key[sKey_.length()];
  
  sKey_.toCharArray(key, sizeof(key));
  
  int keySize = sizeof(key)-1;
    
  String result = "";  // String result = NULL;
  
  int n = response.length();
  
  for(int i=0; i < (n-keySize-1); i++)
  {
    char c[keySize];
    
    for(int k=0; k<keySize; k++)
    {
      c[k] = response.charAt(i+k);
    }
        
    boolean isEqual = true;
    
    for(int k=0; k<keySize; k++)
    {
      if(!(c[k] == key[k]))
      {
        isEqual = false;
        break;
      }
    }
    
    if(isEqual)
    {     
      int j= i + keySize + 1;
      while(!(response.charAt(j) == ','))
      {
        result += response.charAt(j);        
        j++;
      }
      
      //Remove char '"'
      result.replace("\"","");
      break;
    }
  }
  
  return result;
}

void dash()
{
  digitalWrite(LEDpin, HIGH) ;
  delay(DASHLEN);
  digitalWrite(LEDpin, LOW) ;
  delay(DOTLEN) ;
}

void dit()
{
  digitalWrite(LEDpin, HIGH) ;
  delay(DOTLEN);
  digitalWrite(LEDpin, LOW) ;
  delay(DOTLEN);
}

void send(char c)
{
  int i ;
  if (c == ' ') {
    Serial.print(c) ;
    delay(7*DOTLEN) ;
    return ;
  }
  for (i=0; i<N_MORSE; i++) {
    if (morsetab[i].c == c) {
      unsigned char p = morsetab[i].pat ;
      Serial.print(morsetab[i].c) ;

      while (p != 1) {
          if (p & 1)
            dash() ;
          else
            dit() ;
          p = p / 2 ;
      }
      delay(2*DOTLEN) ;
      return ;
    }
  }
  /* if we drop off the end, then we send a space */
  Serial.print("?") ;
}

void sendmsg(char *str)
{
  while (*str)
    send(*str++) ;
  Serial.println("");
}


void getDateTime()
{
  Serial.print("connecting to ");
  Serial.println(timeHost);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  if (!client.connect(timeHost, timePort)) {
    Serial.println("NIST Timeservers: connection failed");
    return;
  }
  
  // This will send the request to the server
  client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");

  delay(100);

  // Read all the lines of the reply from server and print them to Serial
  // expected line is like : Date: Thu, 01 Jan 2015 22:00:14 GMT
  char buffer[12];
  String dateTime = "";

  while(client.available())
  {
    String line = client.readStringUntil('\r');

    if (line.indexOf("Date") != -1)
    {
      Serial.print("=====>");
    } else
    {
      // Serial.print(line);
      // date starts at pos 7
      TimeDate = line.substring(7);
      Serial.println(TimeDate);
      // time starts at pos 14
      TimeDate = line.substring(7, 15);
      TimeDate.toCharArray(buffer, 10);
      Serial.println("UTC Date:");
      Serial.println(buffer);
      //TimeDate = line.substring(16, 24);
      //TimeDate.toCharArray(buffer, 10);
      //Serial.println(buffer);

    }
  }
}

// ---------- ESP 8266 FUNCTIONS - SOME CAN BE REMOVED ----------

void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  

  // ---------- USER CODE GOES HERE ----------

  // ** Transmit Morse Code **
  sendmsg("CALLSIGN HERE"); // FCC callsign and Message
  
  // ** Receive Time (NTP)**
  getDateTime();
  //timeClient.update();
  //Serial.println(timeClient.getFormattedTime()); //prints time in 'hh:mm:ss'
  //delay(1000);

  // ** FireEMS Alert Check **
  FireEmsCheck();

  // ** Weather Alert Check **
  unsigned long currentWxAlertMillis = millis();
  
  if(currentWxAlertMillis - previousWxAlertMillis >= wxAlertCheckInterval) {
    Serial.println("Checking for Weather Alerts");
    WeatherAlerts();
    previousWxAlertMillis = currentWxAlertMillis; //remember the time(millis)
  }
  else {
    Serial.println("Bypassing Weather Alert Check. Less than 15 minutes since last check.");
    Serial.println("Previous Millis: ");
    Serial.println(previousWxAlertMillis);
    Serial.println("Current Millis: ");
    Serial.println(currentWxAlertMillis);
    Serial.println("Subtracted Millis");
    Serial.println(currentWxAlertMillis-previousWxAlertMillis);
    Serial.println();
  }

/*  // ** Weather Check **
  unsigned long currentWxMillis = millis();
  
  if(currentWxMillis - previousWxMillis >= wxCheckInterval) {
    Serial.println("Checking for Weather Alerts");
    WeatherAlerts();
    previousWxMillis = currentWxMillis; //remember the time(millis)
  }
  else {
    Serial.println("Bypassing Weather Check. Less than 15 minutes since last check.");
    Serial.println("Previous Millis: ");
    Serial.println(previousWxMillis);
    Serial.println("Current Millis: ");
    Serial.println(currentWxMillis);
    Serial.println("Subtracted Millis");
    Serial.println(currentWxMillis-previousWxMillis);
    Serial.println();
  }
*/  
  
  // ---------- USER CODE GOES HERE ----------
  yield();
}
