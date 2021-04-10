#if defined(ESP32)
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
//#include <LittleFs.h>
AsyncWebServer server(80);
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
#define APSSID "ESP323232"
#define APPSK "welcome123"
#endif

const char *apssid = APSSID;
const char *appassword = APPSK;

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
int threshold = 10;

String spotPath;

bool checkWifi(const char *SS_ID, const char *Password);
// bool loadConfig();
void setup_AP();
void setup_firebase();
void setup_sensor();
void algorithm();
void changeParkingState(bool state);
bool isConfigured();
void setupWifi();

String ssid, password;
bool configured = false;

int blueLED = 27;
int redLED = 0;
int greenLED = 2;
void setup()
{
  Serial.begin(115200);
  pinMode(blueLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  if (!isConfigured())
  {
    // WiFi.mode(WIFI_STA);
    Serial.println();
    Serial.print("Configuring access point...");
    setup_AP();
  }
  else
  {
    setupWifi();
    setup_firebase();
    changeParkingState(parkingState);
    setup_sensor();
  }
}

void loop()
{
  if (!configured)
  {
    /* code */
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    algorithm();
  }
  else
  {
    setupWifi();
  }
}

void setup_sensor()
{
  Wire.begin(23, 19);
  qmc.init();
  Serial.print(" Steady State measurment");
  int x, y, z;
  float a;
  for (size_t i = 0; i < 300; i++)
  {

    qmc.read(&x, &y, &z, &a);
    init_val += x;
    Serial.print("|");
    delay(25);
  }
  init_val = abs(init_val / 300);
  Serial.println(init_val);
  delay(2000);
}

void setup_firebase()
{
  config.host = FIREBASE_HOST;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

bool checkWifi(const char *SS_ID, const char *Password)
{
  WiFi.disconnect();
  WiFi.begin(SS_ID, Password);
  Serial.println("Waiting for WiFi to connect");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(500);
    return true;
  }
  Serial.print("Failed to Connect \n");
  return false;
}

void processRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String response = "";
  for (size_t i = 0; i < len; i++)
  {
    char character = data[i];
    response += character;
  }
  StaticJsonDocument<200> doc;
  deserializeJson(doc, response);
  JsonObject obj = doc.as<JsonObject>();
  String pSSID = obj["ssid"];
  String pPassword = obj["password"];
  String pPath = obj["path"];
  const char *charSSID = pSSID.c_str();
  const char *charPassword = pPassword.c_str();
  if (checkWifi(charSSID, charPassword))
  {
    //store to flash drive
    spotPath = pPath;
    Serial.print("Wifi Connected\n");
    SPIFFS.begin();
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("Failed to open config file for writing\n");
    }
    else
    {
      request->send(200);
      delay(3000);
      configured = true;
      serializeJson(doc, configFile);
      Serial.print("WiFi Credential Successfully Stored in Flash\n");
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, LOW);
    }
  }
  else
  {
    //send a notification to the user
    request->send(400, "text/html", "You Enter An Incorrect Password/ID");
  }
}

void setup_AP()
{
  digitalWrite(greenLED, HIGH);
  digitalWrite(redLED, HIGH);
  WiFi.softAP(apssid, appassword);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on(
      "/config",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      processRequest);
  server.begin();
  Serial.println("HTTP server started");
}

int x, y, z;
float a;
void algorithm()
{
  qmc.read(&x, &y, &z, &a);
  double diff = abs(abs(x) - init_val) / init_val * 100;
  if (diff > threshold)
  {
    Serial.print(diff);
    Serial.print("-");
    int timeNow = millis();

    if (currentState == true)
    {
      Serial.println("potential car detected");
      bool blink = false;
      while (timeNow + 7000 > millis())
      {
        qmc.read(&x, &y, &z, &a);
        if (abs(abs(x) - init_val) / init_val * 100 < threshold)
        {
          currentState = true;
          Serial.println("Oups not a car");
          digitalWrite(greenLED, HIGH);

          break;
        }
        else
        {
          if (blink)
          {
            digitalWrite(greenLED, LOW);
            blink = !blink;
          }
          else
          {
            digitalWrite(greenLED, HIGH);
            blink = !blink;
          }

          currentState = false;
          Serial.print(".");
          delay(50);
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
      bool blink = false;
      while (timeNow + 7000 > millis())
      {
        qmc.read(&x, &y, &z, &a);
        if (abs(abs(x) - init_val) / init_val * 100 < threshold)
        {
          currentState = true;
          Serial.print(".");
          if (blink)
          {
            digitalWrite(redLED, LOW);
            blink = !blink;
          }
          else
          {
            digitalWrite(redLED, HIGH);
            blink = !blink;
          }
          delay(50);
        }
        else
        {
          currentState = false;
          Serial.println("Car is not leaving, maybe noise or engine startup");
          digitalWrite(redLED, HIGH);
          break;
        }
      }
    }
    else
    {
      digitalWrite(greenLED, HIGH);
      currentState = true;
      Serial.print("|");
      Serial.print(diff);
      Serial.print("|");
      Serial.print("-");
    }
  }
  if (currentState != parkingState)
  {
    Serial.println("Parking is now set to: ");
    parkingState = currentState;
    changeParkingState(parkingState);
    if (parkingState)
    {
      digitalWrite(greenLED, HIGH);
      digitalWrite(redLED, LOW);

      Serial.print("free");
    }
    else
    {

      digitalWrite(redLED, HIGH);
      digitalWrite(greenLED, LOW);

      Serial.print("taken");
    }
  }
  delay(1500);
}

void changeParkingState(bool state)
{
  Serial.println(spotPath);
  if (Firebase.RTDB.setBool(&fbdo, spotPath.c_str(), state))
  {
    //Success
    Serial.println("Set bool data success");
  }
  else
  {
    //Failed?, get the error reason from fbdo

    Serial.print("Error in setBool, ");

    Serial.println(fbdo.errorReason());
  }
}

bool isConfigured()
{
  SPIFFS.begin();

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
    Serial.println("Config file size is too large");
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<200> doc;

  configFile.close();

  DeserializationError error = deserializeJson(doc, buf.get());
  if (error)
  {
    Serial.println("Failed to parse config file");
    Serial.println(error.c_str());
    return false;
  }
  ssid = doc["ssid"].as<String>();
  password = doc["password"].as<String>();
  spotPath = doc["path"].as<String>();
  configured = doc["config"];
  return configured;
}

void setupWifi()
{
  digitalWrite(blueLED, LOW);
  digitalWrite(greenLED, LOW);
  digitalWrite(redLED, LOW);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi ..");
  bool blink = false;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    if (blink)
    {
      digitalWrite(blueLED, LOW);
      blink = !blink;
    }
    else
    {
      digitalWrite(blueLED, HIGH);
      blink = !blink;
    }

    delay(1000);
  }
  Serial.println(WiFi.localIP());
  digitalWrite(blueLED, HIGH);
}