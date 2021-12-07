#include "_config.h"
#include "calibration.h"

#include "SSD1306Wire.h" 
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <EEPROM.h>
#include <string>
#define ESP8266_GPIO4    4 // Relay control.
#define ESP8266_GPIO5    5 // Optocoupler input.

WiFiClient wifiClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

int counter = 0;
int addr = 0;
volatile int inputState = 0;    // Input pin state.
volatile int relayState = 0;    // Relay state
volatile int relayDelayTime = relayDelayInterval;    // Relay state
 
String wifiMacString;
String wifiIPString;
String timeString = "00:00:00";


void handleRoot() {
  char page[800];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  snprintf(page, 800,
           "<html>\
              <head>\
                <meta http-equiv='refresh' content='5'/>\
                <title>Environment Hub</title>\
                <style>\
                  body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
                </style>\
              </head>\
              <body>\
                <p>Time: %s</p>\
                <p>Uptime: %02d:%02d:%02d</p>\
                <p>Input State: %d</p>\
                <p>Relay Delay Time: %d seconds</p>\
              </body>\
            </html>", timeString, hr, min % 60, sec % 60, inputState, relayDelayTime /1000
          );
  httpServer.send(200, "text/html", page);
}
const String postForms = "<html>\
  <head>\
    <title>ESP8266 Web Server POST handling</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Refresh Frequency</h1><br>\
    <form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">\
      <input type=\"text\" name=\"frequency\" value=\""+ String(relayDelayTime / 1000) + "\"><br>\
      <input type=\"submit\" value=\"Submit\">\
    </form>\
  </body>\
</html>";
void handleShowForm() {
  digitalWrite(LED_BUILTIN, 1);
  httpServer.send(200, "text/html", postForms);
  digitalWrite(LED_BUILTIN, 0);
}
void handleForm() {
  if (httpServer.method() != HTTP_POST) {
    digitalWrite(LED_BUILTIN, 1);
    httpServer.send(405, "text/plain", "Method Not Allowed");
    digitalWrite(LED_BUILTIN, 0);
  } else {
    digitalWrite(LED_BUILTIN, 1);
    String message = "POST form was:\n";
    for (uint8_t i = 0; i < httpServer.args(); i++) {
      message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
      if(httpServer.argName(i) == "frequency") {
        relayDelayTime = httpServer.arg(i).toInt() * 1000;
         Serial.println(relayDelayTime);
      }
    }
    httpServer.send(200, "text/plain", message);
    digitalWrite(LED_BUILTIN, 0);
  }
}

void handleNotFound() {
  digitalWrite(LED_BUILTIN, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  digitalWrite(LED_BUILTIN, 0);
}

void setup()
{
    Serial.begin(115200);
      int a;
 int eeAddress = 0; 
  EEPROM.begin(512);
  EEPROM.get(eeAddress, a);
 
  Serial.println(a);
    timeClient.begin();
    
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode( ESP8266_GPIO5, INPUT_PULLUP ); // Input pin.
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);

    Serial.println("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        Serial.print(".");

    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    MDNS.begin(host);

  httpServer.on("/", handleRoot);
  httpServer.on("/postform/", handleForm);
  httpServer.on("/form/", handleShowForm);

  httpServer.onNotFound(handleNotFound);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", host, update_path, update_username, update_password);
    wifiMacString = WiFi.macAddress();
    wifiIPString = WiFi.localIP().toString();

    httpServer.on("/", handleRoot);
}

void loop()
{   
   Serial.println(inputState);
    if(digitalRead(ESP8266_GPIO5) == 0 ) {
      digitalWrite(ESP8266_GPIO4, 0);
      inputState = 1;
      rdi  = millis();
      }

     if (millis() > (relayDelayTime + rdi)) {
        digitalWrite(ESP8266_GPIO4, 1);
        inputState = 0;
        rdi  = millis();
    }

    httpServer.handleClient();
    MDNS.update();

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {
        if (millis() > (ntpInterval + ntpi)) {
          timeClient.update();
          timeString = timeClient.getFormattedTime();
          //Serial.println(timeClient.getFormattedTime());
          ntpi = millis();
        }
        
        HTTPClient http;

        // Your Domain name with URL path or IP address with path
        http.begin(wifiClient, serverName);

        // Specify content-type header
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String httpRequestData = "api_key=" + apiKeyValue;
        httpRequestData = httpRequestData + "&mac_address=" + String(wifiMacString) + "&ip_address=" + String(wifiIPString);

        if (millis() > (sendInterval + si)) {
          digitalWrite(LED_BUILTIN, LOW);
          // uncomment to see the post data in the monitor
          Serial.println(httpRequestData);
          
          // Send HTTP POST request
          int httpResponseCode = http.POST(httpRequestData);

          if (httpResponseCode > 0)
          {
              Serial.print("HTTP Response code: ");
              Serial.println(httpResponseCode);
          }
          else
          {
              Serial.print("Error code: ");
              Serial.println(httpResponseCode);
          }
          si = millis();
           digitalWrite(LED_BUILTIN, HIGH);
        }
        // Free resoclearurces
        http.end();
    }
    else
    {
        Serial.println("WiFi Disconnected");
    }
}
