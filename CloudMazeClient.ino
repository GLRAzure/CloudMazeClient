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

WebSocketsClient webSocket;
const char* chipID;
int count = 0;

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

void connectWS(const char* host, uint16_t port, const char* path = "/", boolean SSL = true) {
    Serial.printf("Connecting to server with ID: %s\n", chipID);
    if (SSL) {
        webSocket.beginSSL(host, port, path, "", chipID);
    } else {
        webSocket.begin(host, port, path, chipID);
    }
    //webSocket.begin("wirelessledgridstream.azurewebsites.net", 80, "/", "devices");
    //webSocket.begin("192.168.23.186", 3001, "/", chipID);
    webSocket.onEvent(webSocketEvent);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WSc] Disconnected!\n");
            webSocket.begin("192.168.23.186", 3001, "/", chipID);
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
    //if (strncmp(type, "world-update", 12)==0) {
        WorldUpdate worldInfo = {
          (const char*)cloudData["player"],
          (const int)cloudData["pos_x"],
          (const int)cloudData["pos_y"],
          (const int)cloudData["player_count"],
          (const char*)cloudData["surroundings"]
        };
        processWorldUpdateMessage(worldInfo);
    //} else {
      
    //}
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
}

void setup() {
    String clientName = "ESP" + String(ESP.getChipId());
    chipID = clientName.c_str();
        
    initSerial();
    initWifi();
    initTime();
    
    connectWS("192.168.23.186",3001);
}

void loop() {
    webSocket.loop();

    if (count < 10) {
      sendActionMessage("move", "north");
    }
    count++;
    delay(20);
}
