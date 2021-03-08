#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include <ressources.h>
#include <MechaQMC5883.h>

/* 1. Define the WiFi credentials */
const char *WIFI_SSID = wifiSSID;
const char *WIFI_PASSWORD = wifiPassword;

/* 2. Define the Firebase project host name and API Key */
std::string FIREBASE_HOST = projectID;
std::string API_KEY = apiKey;

/* 3. Define the user Email and password that alreadey registerd or added in your project */
std::string USER_EMAIL = userEmail;
std::string USER_PASSWORD = userPassword;

/* 4. Define FirebaseESP8266 data object for data sending and receiving */
FirebaseData fbdo;

/* 5. Define the FirebaseAuth data for authentication data */
FirebaseAuth auth;

/* 6. Define the FirebaseConfig data for config data */
FirebaseConfig config;

void changeParkingState(bool state);
void Setup_Firebase();

MechaQMC5883 qmc;

double init_val = 0;
void setup()
{
  Serial.begin(115200);

  Setup_Firebase();

  Wire.begin(23, 19); //  REMOVE THESE TWO ARGUMENTS IF YOUR BOARD HAS PREDEFINED I2C PINS OR SET YOUR CUSTOM ONES HERE

  qmc.init();
  //qmc.setMode(Mode_Continuous,ODR_200Hz,RNG_2G,OSR_256);
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

bool currentState = true;
bool parkingState = true;
int threshold = 20;
void loop()
{
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

void changeParkingState(bool state)
{
  /* 11. Try to set int data to Firebase */
  //The set function returns bool for the status of operation
  //fbdo requires for sending the data and pass as the pointer
  if (Firebase.RTDB.setBool(&fbdo, "/parkings/parking 1/spots/L025", state))
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
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  config.host = FIREBASE_HOST;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}
