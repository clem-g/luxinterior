#include "Arduino.h"
#include <ESP8266WebServer.h>
#include "Adafruit_SSD1306.h"
#include <Adafruit_TSL2561_U.h>

// Wifi credentials
const char* ssid     = "your_ssid";
const char* password = "your_pwd";

// Server port settings
ESP8266WebServer server(80);

// Push buttons
const int buttonAPin =  0;
const int buttonCPin =  2;
volatile boolean deepsleepFlag = false;
volatile boolean settingsFlag  = false;
void buttonA_ISR() {deepsleepFlag = true;}  // Deep sleep mode, wake up using reset
void buttonC_ISR() {settingsFlag  = true;}  // Cycle through gain and integration time settings

// Lux sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT,2561);
uint16_t bb = 0;
uint16_t ir = 0;
uint32_t lx = 0;
String gain = "1x";
String inte = "13.7ms";
  
// OLED screen
#define OLED_RESET 16
Adafruit_SSD1306 display(OLED_RESET);
 
void setup(void) {

  // Push button
  pinMode(buttonAPin, INPUT_PULLUP);
  pinMode(buttonCPin, INPUT_PULLUP);
  attachInterrupt(buttonAPin, buttonA_ISR, FALLING);
  attachInterrupt(buttonCPin, buttonC_ISR, FALLING);
  
  // OLED screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  // Sensor
  tsl.enableAutoRange(false); // No auto-gain for this application
  tsl.setGain(TSL2561_GAIN_1X); // 1X for bright conditions and 16X to boost sensitivity for dim conditions (! saturated sensor will clip and output 0)
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS); // tradeoff with the sigma-delta ADC resolution 13MS (13.7ms) / 101MS / 402MS (full 16bit)
  
  // Connect to WiFi network
  display.print("Connecting to ");
  display.print(ssid);
  display.display();
  WiFi.begin(ssid, password);
  int cnt = 0;
  boolean notConnected = false;
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    display.print(".");
    display.display();
    cnt++;
    if(cnt==3) {
      notConnected = true;
      break;
    }
  }
  
  if(notConnected) {
    display.print("\nNetwork not found.");
  } else {
    display.print("\nWiFi connected at:\n");
    display.print(WiFi.localIP());
    
    server.on("/", [](){   
      server.send(200, "text/plain", "{\n  'illuminance':'"+String(lx)+"',\n  'raw_broadband':'"+String(bb)+"',\n  'raw_infrared':'"+String(ir)+"'\n}");
    });
    
    server.begin();

  }

  display.display();
  delay(4000);
  display.clearDisplay();
  
}
 
void loop(void) {

  // Test if the deep sleep mode interrupt fired, wakeup using reset
  if(deepsleepFlag) {
    deepsleepFlag = false;
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Goodbye..");
    display.display();
    delay(1000);
    tsl.disable();
    display.clearDisplay();
    display.display();
    delay(100);
    ESP.deepSleep(0);
  }

  // Test if the settings interrupt fired, cycle through gains and integration times
  if (settingsFlag) {
    settingsFlag = false;
    if(gain=="1x") {
      gain = "16x";
      tsl.setGain(TSL2561_GAIN_16X);
    } else {
      gain = "1x";
      tsl.setGain(TSL2561_GAIN_1X);
      if(inte=="13.7ms") {
        inte = "101ms";
        tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
      } else if (inte=="101ms") {
        inte = "402ms";
        tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
      } else {
        inte = "13.7ms";
        tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
      }
    }
  }
  
  server.handleClient();
  
  tsl.getLuminosity(&bb,&ir);
  lx = tsl.calculateLux(bb,ir);

  display.clearDisplay();
  display.setCursor(0,0);
  display.print("lux: "); display.print(lx);
  display.print("\nraw: ("); display.print(bb); display.print(";");display.print(ir);display.print(")");
  display.print("\n\n"); display.print(gain); display.print(" / ");display.print(inte);
  display.display();

} 
