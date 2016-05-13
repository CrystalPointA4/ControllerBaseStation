extern "C" {
  #define LWIP_OPEN_SRC
  #include "user_interface.h"
}
#include <Wire.h>
#include <ESP8266WiFi.h>

#define MAX_CLIENTS 4
#define SSIDVISIBLE 0
#define SSIDHIDDEN 1

String WifiSSIDPrefix = "CrystalPoint";
char WifiPassword[10];
char WifiSSID[17];

uint8_t NumberOfConnectedControllers = 0;

ip_addr ConnectionList[MAX_CLIENTS] = {0};

void setup(void){
  Serial.begin(115200);

  //LED
  pinMode(4,OUTPUT);
  digitalWrite(4,LOW);

  //BUTTON
  pinMode(5,INPUT);
  
  delay(100);
  setupWifi();
}

int SSIDVisibleTimeout = 0;
char SSIDVisible = 0;
int NumCurrentConnections = 0;

char ChangeInConnections = 0;
int ChangedConnectionsTimeout = 0;
char DeleteOrAdd = 0;

void loop(void){

  NumCurrentConnections = WiFi.softAPgetStationNum();

  if(NumberOfConnectedControllers != NumCurrentConnections){
      Serial.println("Connections differ");
      ChangeInConnections = 1;
      DeleteOrAdd = NumberOfConnectedControllers < NumCurrentConnections ? 1 : 0;
      NumberOfConnectedControllers = NumCurrentConnections;
  }

  if(ChangeInConnections == 1)
  {
    ChangedConnectionsTimeout++;
  }

  if(ChangeInConnections == 1 && ChangedConnectionsTimeout % 10 == 0)
  {
    int ActualConnections = 0;
    struct station_info *stat_info;
    stat_info = wifi_softap_get_station_info();
    while (stat_info != NULL) {
      ActualConnections++;
      stat_info = STAILQ_NEXT(stat_info, next);
    }

    if(ActualConnections == NumberOfConnectedControllers)
    {
      Serial.println("Connections match");
      ChangeInConnections = 0;
      ChangedConnectionsTimeout = 0;
      RepopulateConnectionList(DeleteOrAdd);
    }
  }

  if(ChangeInConnections == 1 && ChangedConnectionsTimeout > 1000)
  {
    ChangeInConnections = 0;
    ChangedConnectionsTimeout = 0;
  }
  
  if(SSIDVisible == 1)
  {
    SSIDVisibleTimeout++;
  }

  if(SSIDVisible == 1 && SSIDVisibleTimeout > 2000)
  {
    SSIDVisibleTimeout = 0;
    ToggleWifiVisibility(0);
  }
 
  if(digitalRead(5) == 1 && SSIDVisible == 0)
  {
    Serial.println("Button Pressed");
    ToggleWifiVisibility(1);
  }

  delay(100);
}

void ToggleWifiVisibility(char v)
{
  if(v == 1){
    SSIDVisible = 1;
    WiFi.softAP(WifiSSID, WifiPassword, 6, SSIDVISIBLE); 
    digitalWrite(4, HIGH);
    Serial.println("Wifi Visible");
  }else if(v == 0){
    SSIDVisible = 0;
    WiFi.softAP(WifiSSID, WifiPassword, 6, SSIDHIDDEN); 
    digitalWrite(4, LOW);
    Serial.println("Wifi Hidden");
  }
}

void RepopulateConnectionList(char n)
{
  struct station_info *stat_info;
  stat_info = wifi_softap_get_station_info();
  
  //Controller disconnected
  if(n == 0){
    Serial.println("Old controller disconnected..");
    for(char i=0; i < MAX_CLIENTS; i++){
      char missingIP = 0;
      while (stat_info != NULL) {
        if(ip_addr_cmp(&ConnectionList[i], &stat_info->ip)){
           missingIP = 255;
           break;
        }else{
          missingIP = i;
        }

        //str += IPAddress((stat_info->ip).addr).toString();
        stat_info = STAILQ_NEXT(stat_info, next);
      }
 if(missingIP != 255 && ConnectionList[missingIP].addr != 0){
        //Ip NOT found; This controller is gone. 
        ConnectionList[missingIP].addr = 0;
        Serial.printf("Controller %d disconnected \n\r", missingIP);
      }
    }
  }
  //new controller
  else if(n == 1){
    Serial.println("New controller connected..");
    while (stat_info != NULL){
      Serial.println(IPAddress((stat_info->ip).addr).toString());
      char addedIP = 0;
      for(char i=0; i < MAX_CLIENTS; i++){
        Serial.println(ip_addr_cmp(&ConnectionList[i], &stat_info->ip));
        if(ip_addr_cmp(&ConnectionList[i], &stat_info->ip)){
           addedIP = 255;
           break;
        }else{
          addedIP = i;
        }
      }
      if(addedIP != 255){
        //Ip NOT found; This controller is new. 
        for(char i=0; i < MAX_CLIENTS; i++){
          if(ConnectionList[i].addr == 0){
            ConnectionList[i] = stat_info->ip;
            Serial.printf("Controller connect, place of index: %d \n\r", i);
            break;
          }
        }
      }
     Serial.println("---");
     stat_info = STAILQ_NEXT(stat_info, next);
  }
} 
}

void setupWifi(void){
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);

  generateWiFiSSID();
  generateWiFiPassword();

  WiFi.softAP(WifiSSID, WifiPassword, 6, SSIDVISIBLE);  
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
  Serial.print("Wifi password: ");  
  Serial.println(WifiPassword);
}
