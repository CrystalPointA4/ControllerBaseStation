extern "C" {
  #define LWIP_OPEN_SRC
  #include "user_interface.h"
}
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define MAX_CLIENTS 4

String WifiSSIDPrefix = "CrystalPoint";
char WifiPassword[10];
char WifiSSID[17];

struct controllerInformation{
  IPAddress ipaddr;
  u_long lastPackage;
};

controllerInformation ConnectionList[MAX_CLIENTS];
u_long lastDisconnectionCheck = 0;

//Serial buffer string
String serialInput = "";
boolean serialDataReady = false;

//UDP server (receiving) setup
unsigned int udpPort = 2730;
char packetBuffer[50]; //udp package buffer
WiFiUDP Udp;
boolean started = false;

void setup(void){
  Serial.begin(115200);
  Serial.println();

  //LED
  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);

  //BUTTON
  pinMode(0,INPUT);

  Udp.begin(udpPort);
  
  delay(100);
  setupWifi();

  serialInput.reserve(200);
}

int SSIDVisibleTimeout = 0;
char SSIDVisible = 0;

void loop(void){
  int noBytes = Udp.parsePacket();
  if ( noBytes ) {
    Udp.read(packetBuffer,noBytes); 
    if(started){
      Serial.printf("1|%d|", getControllerId(Udp.remoteIP()));
      for (int i=1;i<=noBytes;i++){
        Serial.printf("%c", packetBuffer[i-1]); 
      }
      Serial.println();
    }else{
      getControllerId(Udp.remoteIP());
    }
  }  

  if (serialDataReady) {
    Serial.println("0|" + serialInput);
    if(serialInput == "start"){
      started = true;
    }else if(serialInput == "stop"){
      started = false;
    }else if(serialInput == "clientlist"){
      getControllerList();
    }else if(serialInput[0] == '1' && serialInput.length() == 14){ //Rumble
      char controllerId = atoi(&serialInput[2]);
      if(isControllerConnected(controllerId) == 1){
        Serial.println("0|Controller connected, rumble sended");
          serialInput.toCharArray(packetBuffer, 50);
          Udp.beginPacket(ConnectionList[controllerId].ipaddr, udpPort);
          Udp.write(packetBuffer);
          Udp.endPacket();
      }else{
        Serial.println("0|Controller not connected");
      }
    }
    serialInput = "";
    serialDataReady = false;
  }
  if (Serial.available()) serialEvent();

  if(SSIDVisible == 1 && (SSIDVisibleTimeout + 30000) < millis())
  {
    SSIDVisibleTimeout = 0;
    ToggleWifiVisibility(0);
  }
 
  if(digitalRead(0) == 0 && SSIDVisible == 0)
  {
    Serial.println("0|Button Pressed");
    SSIDVisibleTimeout = millis();
    ToggleWifiVisibility(1);
  }

  if(millis() - lastDisconnectionCheck > 10000){
    checkControllerDisconnect();
  }
  
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      serialDataReady = true;
    }else{
      serialInput += inChar;
    }
  }
}

void ToggleWifiVisibility(char v)
{
  if(v == 1){
    SSIDVisible = 1;
    WiFi.softAP(WifiSSID, WifiPassword, 6, 0); 
    digitalWrite(2, HIGH);
    Serial.println("0|Wifi Visible");
  }else if(v == 0){
    SSIDVisible = 0;
    WiFi.softAP(WifiSSID, WifiPassword, 6, 1); 
    digitalWrite(2, LOW);
    Serial.println("0|Wifi Hidden");
  }
}

char getControllerId(IPAddress ipaddr){
  for(char i=0; i < MAX_CLIENTS; i++){  
    if(ConnectionList[i].ipaddr == ipaddr){ //Yay, controller found. Return the id.
      ConnectionList[i].lastPackage = millis();
      return i;  
    }
  }
  //We didn't found the controller, so add it and return a new id.
  for(char i=0; i < MAX_CLIENTS; i++){  
    if(ConnectionList[i].ipaddr == INADDR_NONE){ 
      ConnectionList[i].ipaddr = ipaddr;
      ConnectionList[i].lastPackage = millis();
      Serial.printf("2|connected|%d\n\r", i);
      return i;  
    }
  }
  //No room for new controllers. 
  return -1;  
}

void checkControllerDisconnect(){
  for(char i=0; i < MAX_CLIENTS; i++){  
    if(ConnectionList[i].ipaddr != INADDR_NONE && millis() - ConnectionList[i].lastPackage > 10000){  //Controller didn't send any messages last 10 seconds, disconnected.
      ConnectionList[i].ipaddr = INADDR_NONE; 
      Serial.printf("2|disconnected|%d\n\r", i);
    }
  }  
}

char isControllerConnected(char id){
  if(id <= MAX_CLIENTS && id >= 0 && ConnectionList[id].ipaddr != INADDR_NONE ){ 
      return 1;
  }
  return 0;    
}

void setupWifi(void){
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);

  generateWiFiSSID();
  generateWiFiPassword();

  WiFi.softAP(WifiSSID, WifiPassword, 6, 1);  
}

void generateWiFiSSID()
{
  //Last 4 digits mac adress
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  WifiSSIDPrefix += macID;

  memset(WifiSSID, 0, WifiSSIDPrefix.length() + 1);

  for (int i=0; i<WifiSSIDPrefix.length(); i++)
    WifiSSID[i] = WifiSSIDPrefix.charAt(i);
}

void generateWiFiPassword()
{
  String WifiStringPassword;
  for(char i=0; i < 17; i++){
      String s = String(WifiSSID[i], HEX);
      WifiStringPassword = s + WifiStringPassword;
  }
  for (int i=0; i < 10; i++)
    WifiPassword[i] = WifiStringPassword.charAt(i);
  WifiPassword[9] = '\0';
  Serial.print("0|Wifi password: ");  
  Serial.println(WifiPassword);
}


//Serial commands

void getControllerList(){
  Serial.printf("3|");
  for(char i=0; i < MAX_CLIENTS; i++){  
    if(ConnectionList[i].ipaddr != INADDR_NONE){ 
      Serial.printf("%d|", i);
    }
  }
  Serial.println();  
}
