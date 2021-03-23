#if defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
//#include <LittleFs.h>
WebServer server(80);
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFs.h>
ESP8266WebServer server(80);
#endif
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include "ressources.h"
#include <MechaQMC5883.h>

#include <ArduinoJson.h>
#include "FS.h"

#ifndef APSSID
#define APSSID "ESP-WIFI"
#define APPSK  "123456789"
#endif

const char *apssid = APSSID;
const char *appassword = APPSK;
bool flag1 = true;
bool flag2 = true;

const char *WIFI_SSID = wifiSSID;
const char *WIFI_PASSWORD = wifiPassword;
std::string FIREBASE_HOST = projectID;
std::string API_KEY = apiKey;
std::string USER_EMAIL = userEmail;
std::string USER_PASSWORD = userPassword;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

MechaQMC5883 qmc;
double init_val = 0;
bool currentState = true;
bool parkingState = true;
int threshold = 20;

void changeParkingState(bool state);
void Setup_Firebase();
void algorithm();
void saveConfig();
bool checkWifi(const char* SS_ID,const char* Password);
bool loadConfig();
void AP_setup();
void sensor_setup();

void setup()
{
  Serial.begin(115200);
  if(loadConfig()){
    WiFi.mode(WIFI_STA);
    Serial.println();
    Serial.print("Configuring access point...");
    AP_setup();
  }
  else{
    Setup_Firebase();
  }
  
}

void loop()
{
  if(WiFi.status() == WL_CONNECTED){
    if(flag1){
      flag1 = false;
      flag2 = true;
      WiFi.softAPdisconnect (true);
    }
    algorithm();
  }
  else{
    if(flag2){
      flag1 = true;
      flag2 = false;
      if(WiFi.status() != WL_CONNECTED){
        Serial.print("Waiting to make a reconnection \n");
        for(int i =0;i<20;i++){
          Serial.print(".");
          delay(10);
        }
      }
      if(WiFi.status() == WL_CONNECTED){
        return;
      }
      AP_setup();
    }
    server.handleClient();
  }
  
}

void changeParkingState(bool state)
{
  SPIFFS.begin();
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<200> doc;
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
  }
  String Path = doc["path"];
  Serial.println(Path);
  const char* P = Path.c_str();
  
  if (Firebase.RTDB.setBool(&fbdo, P, state))
  {
    //Success
    Serial.println("Set int data success");
  }
  else
  {
    //Failed?, get the error reason from fbdo

    Serial.print("Error in setBool, ");

    Serial.println(fbdo.errorReason());
  }
}

void Setup_Firebase()
{
  config.host = FIREBASE_HOST;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void algorithm(){
  int x, y, z;
  float a;

  qmc.read(&x, &y, &z, &a);
  double diff = abs(x - init_val) / init_val * 100;
  if (diff > threshold)
  {
    int timeNow = millis();

    if (currentState == true)
    {
      Serial.println("potential car detected");
      while (timeNow + 7000 > millis())
      {
        qmc.read(&x, &y, &z, &a);
        if (abs(x - init_val) / init_val * 100 < threshold)
        {
          currentState = true;
          Serial.println("Oups not a car");
          break;
        }
        else
        {
          currentState = false;
          Serial.print(".");
          delay(10);
        }
      }
      Serial.println();
    }
  }
  else
  {
    if (currentState == false)
    {
      int timeNow = millis();
      Serial.println("potential car leaving");
      while (timeNow + 7000 > millis())
      {
        qmc.read(&x, &y, &z, &a);
        if (abs(x - init_val) / init_val * 100 < threshold)
        {
          currentState = true;
          Serial.print(".");
          delay(10);
        }
        else
        {
          currentState = false;
          Serial.println("Car is not leaving, maybe noise or engine startup");
          break;
        }
      }
    }
    else
    {
      currentState = true;
      Serial.print(diff);
      Serial.print("-");
    }
  }
  if (currentState != parkingState)
  {
    // changeParkingState(currentState);
    Serial.println("Parking is now set to: ");
    parkingState = currentState;
    changeParkingState(parkingState);
    if (parkingState)
    {
      Serial.print("free");
    }
    else
    {
      Serial.print("taken");
    }
  }
  delay(1000);
}

void saveConfig(){
  //Processing the Json String
  StaticJsonDocument<200> doc;
  String ssid1 = server.arg("plain");
  deserializeJson(doc, ssid1);
  JsonObject obj = doc.as<JsonObject>();
  String SS_ID = obj["ssid"];
  String Password = obj["password"];
  String Path = obj["path"];
  const char* SS = SS_ID.c_str();
  const char* PP = Password.c_str();
  if(checkWifi(SS,PP)){
    //store to flash drive
    Serial.print("Wifi Connected\n");
    SPIFFS.begin();
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing\n");
    }
    else{
    serializeJson(doc, configFile);
    Serial.print("WiFi Credential Successfully Stored in Flash\n");
    }
  }
  else{
    //send a notification to the user
    server.send(404,"text/html","You Enter An Incorrect Password/ID");
  }
}
bool checkWifi(const char* SS_ID,const char* Password)
{
  WiFi.disconnect();
  WiFi.begin(SS_ID,Password);
  int c = 0;
  Serial.println("Waiting for WiFi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.print("Failed to Connect \n");
  return false;
}
bool loadConfig() {
  SPIFFS.begin();
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<200> doc;
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
  }

  String SS_ID = doc["ssid"];
  String Password = doc["password"];
  String Path = doc["path"];
  const char* SS = SS_ID.c_str();
  const char* PP = Password.c_str();
  if(checkWifi("1",PP)){
     return false;
  }
  else{
    return true;
  }
}
void AP_setup(){
  WiFi.softAP(apssid, appassword);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.on("/config", saveConfig);
    server.begin();
    Serial.println("HTTP server started");
}
void sensor_setup(){
  Wire.begin(23, 19); 
  qmc.init();
  Serial.print(" Steady State measurment");
  for (size_t i = 0; i < 300; i++)
  {
    int x, y, z;
    float a;

    qmc.read(&x, &y, &z, &a);
    init_val += x;
    Serial.print(".");
    delay(25);
  }
  init_val /= 300;
  Serial.println(init_val);
  delay(2000);
}