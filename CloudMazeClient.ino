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

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include <Pushbutton.h>

Pushbutton upButton(0);
Pushbutton downButton(2);
Pushbutton leftButton(14);
Pushbutton rightButton(12);
Pushbutton aButton(16);
Pushbutton bButton(13);

bool upPressed = false;
bool downPressed = false;
bool leftPressed = false;


WebSocketsClient webSocket;
const char* chipID;
const char* wsHost = "192.168.1.249";
uint32_t wsPort = 3001;
int count = 0;

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(3, 3, 15,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB            + NEO_KHZ800);

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

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //the line below disables the wifi debug messages
    wifiManager.setDebugOutput(false);
    //reset saved settings
    //wifiManager.resetSettings();

    //wifiManager.autoConnect("AutoConnectAP");
    //or use this for auto generated name ESP + ChipID
    wifiManager.autoConnect();

    Serial.println("Connected to wifi");
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
  drawMatrix(worldInfo.surroundings);
}

void drawMatrix(const char* surroundings) {
    uint32_t w = matrix.Color(255, 0, 0);

    matrix.fillScreen(matrix.Color(0,0,0));

    //matrix.drawPixel(0, 0, w);
    //matrix.drawPixel(1, 0, w);
    //matrix.drawPixel(2, 0, w);
    //matrix.drawPixel(0, 1, matrix.Color(0, 255, 0));
    //matrix.drawPixel(1, 1, matrix.Color(0, 255, 0));
    //matrix.drawPixel(2, 1, matrix.Color(0, 255, 0));
    //matrix.drawPixel(0, 2, matrix.Color(0, 0, 255));
    //matrix.drawPixel(1, 2, matrix.Color(0, 0, 255));
    //matrix.drawPixel(2, 2, matrix.Color(0, 0, 255));

    matrix.show();
}

void processButtons() {
  if (upButton.getSingleDebouncedPress())
  {
    Serial.println("Up Pressed");
    sendActionMessage("move", "north");
  }
  if (downButton.getSingleDebouncedPress())
  {
    Serial.println("Down Pressed");
    sendActionMessage("move", "south");
  }
  if (leftButton.getSingleDebouncedPress())
  {
    Serial.println("Left Pressed");
    sendActionMessage("move", "west");
  }
  if (rightButton.getSingleDebouncedPress())
  {
    Serial.println("Right Pressed");
    sendActionMessage("move", "east");
  }
  if (aButton.getSingleDebouncedPress())
  {
    Serial.println("A Pressed");
  }
  if (bButton.getSingleDebouncedPress())
  {
    Serial.println("B Pressed");
  }
}

void setup() {
    String clientName = "ESP" + String(ESP.getChipId());
    chipID = clientName.c_str();
        
    initSerial();
    initWifi();
    initTime();

    matrix.begin();
    matrix.setBrightness(40);
    
    connectWS(wsHost,wsPort);
}

void loop() {
    webSocket.loop();
    
    //if (count < 10) {
    //  sendActionMessage("move", "north");
    //}
    processButtons();
    //count++;
    delay(20);
}
