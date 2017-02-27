/* 
 * Brian Leschke
 * February 27, 2017
 * Adafruit Huzzah ESP 8266 Neopixel Light
 * The ESP8266 will control a neopixel and change the color based on Weather events, Holidays, and Fire/EMS calls.
 * Version 1.1
 * 
 * -- Changelog: -- 
 * 
 * 2/26/17 - Initial Release - Fire/EMS and Weather Alerts implemented
 * 2/27/17 - Added NTP Client and Hellschreiber
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
const String myKey             = "API-KEY HERE";         //See: http://www.wunderground.com/weather/api/d/docs (change here with your KEY)
const String myWxAlertFeatures = "alerts";               // Do Not Change. See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
const String myWxFeatures      = "conditions";           // Do Not Change. See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
const String myState           = "ABBREVIATED STATE";    //See: http://www.wunderground.com/weather/api/d/docs?d=resources/country-to-iso-matching
const String myCity            = "CITY";                 //See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1

long wxAlertCheckInterval  = 900000; // 15min default. Time (milliseconds) until next weather alert check
unsigned long previousWxAlertMillis = 0;
// long wxCheckInterval  = 900000; // 15min default. Time (milliseconds) until next weather check
// unsigned long previousWxMillis = 0;


// ** FIRE-EMS INFORMATION **
char SERVER_NAME[]    = "WEB ADDRESS HERE"; // Address of the webserver
int SERVER_PORT       = PORT NUMBER HERE;       // webserver port

char Str[11];
int prevNum           = 0; //Number of previous emails before check
int num               = 0; //Number of emails after check


// ** NTP SERVER INFORMATION **
WiFiUDP ntpUDP;
// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
NTPClient timeClient(ntpUDP); // default 'time.nist.gov'

// ** HELLSCHREIBER TRANSMISSION INFORMATION **
#define NGLYPHS         (sizeof(glyphtab)/sizeof(glyphtab[0]))
int radioPin = 2;
char callsign[] = "CALLSIGN HERE"; // FCC Callsign


typedef struct glyph {
    char ch ;
    word col[7] ;
} Glyph ;
 
Glyph glyphtab[] PROGMEM = {
{' ', {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}},
{'A', {0x07fc, 0x0e60, 0x0c60, 0x0e60, 0x07fc, 0x0000, 0x0000}},
{'B', {0x0c0c, 0x0ffc, 0x0ccc, 0x0ccc, 0x0738, 0x0000, 0x0000}},
{'C', {0x0ffc, 0x0c0c, 0x0c0c, 0x0c0c, 0x0c0c, 0x0000, 0x0000}},
{'D', {0x0c0c, 0x0ffc, 0x0c0c, 0x0c0c, 0x07f8, 0x0000, 0x0000}},
{'E', {0x0ffc, 0x0ccc, 0x0ccc, 0x0c0c, 0x0c0c, 0x0000, 0x0000}},
{'F', {0x0ffc, 0x0cc0, 0x0cc0, 0x0c00, 0x0c00, 0x0000, 0x0000}},
{'G', {0x0ffc, 0x0c0c, 0x0c0c, 0x0ccc, 0x0cfc, 0x0000, 0x0000}},
{'H', {0x0ffc, 0x00c0, 0x00c0, 0x00c0, 0x0ffc, 0x0000, 0x0000}},
{'I', {0x0ffc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}},
{'J', {0x003c, 0x000c, 0x000c, 0x000c, 0x0ffc, 0x0000, 0x0000}},
{'K', {0x0ffc, 0x00c0, 0x00e0, 0x0330, 0x0e1c, 0x0000, 0x0000}},
{'L', {0x0ffc, 0x000c, 0x000c, 0x000c, 0x000c, 0x0000, 0x0000}},
{'M', {0x0ffc, 0x0600, 0x0300, 0x0600, 0x0ffc, 0x0000, 0x0000}},
{'N', {0x0ffc, 0x0700, 0x01c0, 0x0070, 0x0ffc, 0x0000, 0x0000}},
{'O', {0x0ffc, 0x0c0c, 0x0c0c, 0x0c0c, 0x0ffc, 0x0000, 0x0000}},
{'P', {0x0c0c, 0x0ffc, 0x0ccc, 0x0cc0, 0x0780, 0x0000, 0x0000}},
{'Q', {0x0ffc, 0x0c0c, 0x0c3c, 0x0ffc, 0x000f, 0x0000, 0x0000}},
{'R', {0x0ffc, 0x0cc0, 0x0cc0, 0x0cf0, 0x079c, 0x0000, 0x0000}},
{'S', {0x078c, 0x0ccc, 0x0ccc, 0x0ccc, 0x0c78, 0x0000, 0x0000}},
{'T', {0x0c00, 0x0c00, 0x0ffc, 0x0c00, 0x0c00, 0x0000, 0x0000}},
{'U', {0x0ff8, 0x000c, 0x000c, 0x000c, 0x0ff8, 0x0000, 0x0000}},
{'V', {0x0ffc, 0x0038, 0x00e0, 0x0380, 0x0e00, 0x0000, 0x0000}},
{'W', {0x0ff8, 0x000c, 0x00f8, 0x000c, 0x0ff8, 0x0000, 0x0000}},
{'X', {0x0e1c, 0x0330, 0x01e0, 0x0330, 0x0e1c, 0x0000, 0x0000}},
{'Y', {0x0e00, 0x0380, 0x00fc, 0x0380, 0x0e00, 0x0000, 0x0000}},
{'Z', {0x0c1c, 0x0c7c, 0x0ccc, 0x0f8c, 0x0e0c, 0x0000, 0x0000}},
{'0', {0x07f8, 0x0c0c, 0x0c0c, 0x0c0c, 0x07f8, 0x0000, 0x0000}},
{'1', {0x0300, 0x0600, 0x0ffc, 0x0000, 0x0000, 0x0000, 0x0000}},
{'2', {0x061c, 0x0c3c, 0x0ccc, 0x078c, 0x000c, 0x0000, 0x0000}},
{'3', {0x0006, 0x1806, 0x198c, 0x1f98, 0x00f0, 0x0000, 0x0000}},
{'4', {0x1fe0, 0x0060, 0x0060, 0x0ffc, 0x0060, 0x0000, 0x0000}},
{'5', {0x000c, 0x000c, 0x1f8c, 0x1998, 0x18f0, 0x0000, 0x0000}},
{'6', {0x07fc, 0x0c66, 0x18c6, 0x00c6, 0x007c, 0x0000, 0x0000}},
{'7', {0x181c, 0x1870, 0x19c0, 0x1f00, 0x1c00, 0x0000, 0x0000}},
{'8', {0x0f3c, 0x19e6, 0x18c6, 0x19e6, 0x0f3c, 0x0000, 0x0000}},
{'9', {0x0f80, 0x18c6, 0x18cc, 0x1818, 0x0ff0, 0x0000, 0x0000}},
{'*', {0x018c, 0x0198, 0x0ff0, 0x0198, 0x018c, 0x0000, 0x0000}},
{'.', {0x001c, 0x001c, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}},
{'?', {0x1800, 0x1800, 0x19ce, 0x1f00, 0x0000, 0x0000, 0x0000}},
{'!', {0x1f9c, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}},
{'(', {0x01e0, 0x0738, 0x1c0e, 0x0000, 0x0000, 0x0000, 0x0000}},
{')', {0x1c0e, 0x0738, 0x01e0, 0x0000, 0x0000, 0x0000, 0x0000}},
{'#', {0x0330, 0x0ffc, 0x0330, 0x0ffc, 0x0330, 0x0000, 0x0000}},
{'$', {0x078c, 0x0ccc, 0x1ffe, 0x0ccc, 0x0c78, 0x0000, 0x0000}},
{'/', {0x001c, 0x0070, 0x01c0, 0x0700, 0x1c00, 0x0000, 0x0000}},
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
  pinMode(radioPin, OUTPUT) ; // Initialize Hellschreiber transmission output.
  
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
  timeClient.begin(); // Start NTP client

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
    Serial.println("Connection to Wunderground failed!");
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
    
    pixels.setPixelColor(0, pixels.Color(255,255,255)); // DEFAULT WHITE
    pixels.show(); // This sends the updated pixel color to the hardware.
    Serial.println("No Reportable Weather Alerts");
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

void encodechar(int ch)
{
    int i, x, y, fch ;
    word fbits ;
 
    /* It looks sloppy to continue searching even after you've
     * found the letter you are looking for, but it makes the 
     * timing more deterministic, which will make tuning the 
     * exact timing a bit simpler.
     */
    for (i=0; i<NGLYPHS; i++) {
        fch = pgm_read_byte(&glyphtab[i].ch) ;
        if (fch == ch) {
            for (x=0; x<7; x++) {
                fbits = pgm_read_word(&(glyphtab[i].col[x])) ;
                for (y=0; y<14; y++) {
                    if (fbits & (1<<y))
                        digitalWrite(radioPin, HIGH) ;
                    else
                        digitalWrite(radioPin, LOW) ;
                         
                    delayMicroseconds(4045L) ;
                }
            }
        }
    }
}
 
void encode(char *ch)
{
    while (*ch != '\0') 
        encodechar(*ch++) ;
}


// ---------- ESP 8266 FUNCTIONS - SOME CAN BE REMOVED ----------

void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  

  // ---------- USER CODE GOES HERE ----------

  // ** Receive Time (NTP) **
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());  //prints time in 'hh:mm:ss'
  delay(1000);
  
  // ** Transmit HellSchreiber **
  encode(callsign);
  
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

