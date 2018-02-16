#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <WebSocketsClient.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Pushbutton.h>

Pushbutton upButton(0);
Pushbutton downButton(2);
Pushbutton leftButton(14);
Pushbutton rightButton(12);
Pushbutton aButton(13);
Pushbutton bButton(16);

const uint8_t ledGridWidth = 3;
const uint8_t ledGridHeight = 3;

WebSocketsClient webSocket;
const char* chipID;
const char* wsHost = "52.224.239.175";
uint32_t wsPort = 80;
int count = 0;

const bool useWifiManager = false;
const char* wifiSsid = "techo2018";
const char* wifiPassword = "techo2018";

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(9, 15, NEO_GRB + NEO_KHZ800);

struct WorldUpdate {
  const char* player;
  const int pos_x;
  const int pos_y;
  const int player_count;
  const char* surroundings;
};

void initSerial() {
  // Start serial and initialize stdout
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
}
void initWifi() {
  if (useWifiManager == true) {
    //WiFiManager
    WiFiManager wifiManager;
    //the line below disables the wifi debug messages
    wifiManager.setDebugOutput(true);
    //reset saved settings
    //wifiManager.resetSettings();

    wifiManager.autoConnect();
    Serial.println("Connected to wifi");
  } else {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(wifiSsid);
    
    WiFi.begin(wifiSsid, wifiPassword);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());    
  }
}

void initTime() {
  time_t epochTime;
  int retries = 0;
  
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  while (retries < 30) {
    epochTime = time(NULL);

    if (epochTime == 0) {
      Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
      delay(2000);
      retries++;
    } else {
      Serial.print("Fetched NTP epoch time is: ");
      Serial.println(epochTime);
      break;
    }
  }
}

void connectWS(const char* host, uint16_t port = 80, const char* path = "/", boolean SSL = false) {
  Serial.printf("Connecting to server with ID: %s\n", chipID);
  if (SSL) {
    webSocket.beginSSL(host, port, path, "", chipID);
  } else {
    webSocket.begin(host, port, path, chipID);
  }
  webSocket.onEvent(webSocketEvent);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      connectWS(wsHost,wsPort);
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n",  payload);
      // send message to server when Connected
	    //webSocket.sendTXT("{\"action\":\"Client join\"}");
      break;
    case WStype_TEXT:
			Serial.printf("Received update from cloud: %s\n", payload);
			processCloudMessage((char*)payload);
			break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);
      break;
  }
}

void processCloudMessage(char* cloudMessage) {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& cloudData = jsonBuffer.parseObject(cloudMessage);
  if (!cloudData.success()) {
    Serial.println("Malformed JSON from cloud. Blame Andrej");
    return;
  }
  Serial.print("CloudMessage Type: ");
  const char* type = (const char*)cloudData["type"];
  Serial.println(type);
  if (strcmp(type, "world-update") == 0) {
      WorldUpdate worldInfo = {
        (const char*)cloudData["player"],
        (const int)cloudData["pos_x"],
        (const int)cloudData["pos_y"],
        (const int)cloudData["player_count"],
        (const char*)cloudData["surroundings"]
      };
      processWorldUpdateMessage(worldInfo);
  } else {

  }
}

void sendActionMessage(char* action, char* direction) {
  StaticJsonBuffer<200> jsonBuffer;
  char json[200];
  JsonObject& message = jsonBuffer.createObject();
  message["action"] = action;
  message["direction"] = direction;
  message.printTo(json, sizeof(json));
  webSocket.sendTXT(json);
  Serial.println(json);
}

void processWorldUpdateMessage(WorldUpdate worldInfo) {
  Serial.println(worldInfo.player);
  Serial.println(worldInfo.pos_x);
  Serial.println(worldInfo.pos_y);
  Serial.println(worldInfo.player_count);
  Serial.println(worldInfo.surroundings);
  drawPixels(worldInfo.surroundings);
}

void setPixelCoordColor(uint8_t x, uint8_t y, uint32_t color) {
  pixels.setPixelColor((y * ledGridWidth) + x, color);
}

void drawPixels(const char* surroundings) {1
  for(int i=0;i<9;i++){
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    switch(surroundings[i]) {
      case 'w':
        pixels.setPixelColor(i, pixels.Color(50,0,0));
        break;
      default:
        pixels.setPixelColor(i, pixels.Color(0,0,0));
        break;
    }
  }
  setPixelCoordColor(1,1, pixels.Color(50,50,50));
  pixels.show(); // This sends the updated pixel color to the hardware.
}

void processButtons() {
  if (upButton.getSingleDebouncedPress()) {
    Serial.println("Up Pressed");
    sendActionMessage("move", "north");
  }
  if (downButton.getSingleDebouncedPress()) {
    Serial.println("Down Pressed");
    sendActionMessage("move", "south");
  }
  if (leftButton.getSingleDebouncedPress()) {
    Serial.println("Left Pressed");
    sendActionMessage("move", "west");
  }
  if (rightButton.getSingleDebouncedPress()) {
    Serial.println("Right Pressed");
    sendActionMessage("move", "east");
  }
  if (aButton.getSingleDebouncedPress()) {
    Serial.println("A Pressed");
  }
  if (bButton.getSingleDebouncedPress()) {
    Serial.println("B Pressed");
  }
}

void setup() {
  String clientName = "ESP" + String(ESP.getChipId());
  chipID = clientName.c_str();
      
  initSerial();
  initWifi();
  initTime();

  pixels.begin();
  
  connectWS(wsHost,wsPort);
}

void loop() {
  webSocket.loop();
  processButtons();
}
