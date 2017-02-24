# Huzzah-ESP8266-Neopixel-Light
Brian Leschke 2017

## **Overview**

The ESP8266 will control a neopixel and change the color based on Weather events, Holidays, and Fire/EMS calls. 

### **Prerequisities**

You will need:

1. Adafruit Huzzah ESP8266 (http://adafru.it/2471)
2. Flora Neopixel (http://adafru.it/1312)
3. PlatformIO
4. Operating system that supports PlatformIO
5. FTDI cable (https://www.sparkfun.com/products/9717)

### **Usage**

In order to use successfully implement this project and its code, you will need to install PlatformIO.
Installation instructions can be found at:

    http://docs.platformio.org/en/latest/installation.html
    
### **Libraries**

The following libraries are required for this project:
    
  * Arduino Framework (core library)
  * Adafruit IO Arduino 
  * Adafruit MQTT 
  * NeoPixelBus
  * OLED
    * Adafruit GFX Library
    * Adafruit SSD1306 Library
        
## **Uploading**

The code can be uploaded to the ESP8266 in two ways, either by OTA (over-the-air) updates or by a serial (FTDI cable) connection. 
Users will need to initially upload the code using an FTDI cable.

### **FTDI Upload**

This method must be used in the initial flashing of the ESP8266. An FTDI or UART cable has 6 pins.
Every time the you want to flash the ESP8266, you will need to do the following to enter the bootloader mode:

  1. Press and hold the RESET button.
  2. Press and hold the GPIO0 button.
  3. Release the RESET button and continue to hold the GPIO0 button.
  4. Release the GPIO0 button (you should see a faint red glow from the GPIO0 LED).
  
The stock firmware is SPIFFS filesystem. Data to be written to this portion of the filesystem includes:

    WiFi SSID and Password
  
To upload the SPIFFS image, enter the following into a terminal window:

    platformio run -t uploadfs
  
When the upload is complete (blue light ceases to blink), the system will automatically reboot. 

To upload the main.cpp code to the ESP8266, enter the following into a terminal window:
  
    platformio run -t upload

### **OTA Upload**

Over-the-air updates is a method to update the ESP8266 without physically being with the device. To update the device you will need to
know the IP address and be connected to the same network.

If for some reason, the network ESP8266 network configuration is incorrect, the device will enter setup mode.
In setup mode, the device will create it's own access point with a default IP address of "192.168.13.1" and a default SSID of"esp8266-".

To upload the SPIFFS image, enter the following into a terminal window:

    platformio run -t uploadfs --upload-port <ESP8266_IP>
    
To upload the main.cpp code to the ESP8266, enter the following into a terminal window:
    
    platformio run -t upload --upload-port <ESP8266_IP>

        
### **Recognition and Credit**
I would like to recognize and give credit to [cjntaylor](https://github.com/cjntaylor/) for some of the code, ideas, and concepts that were used in the maccdc2016 repository. 

