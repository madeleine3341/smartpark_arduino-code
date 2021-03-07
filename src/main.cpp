#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include <ressources.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

#define trig_TH 1000 // Thresh hold level to trigger the algorithm
#define avg_TH 70
#define S 300 // How many samples to take to decide if the car is parked

//String type = "QMC5883L";
String type = "HMC5883L";            //uncomment this line, and comment the line above if using the HMC5883L
String Parking_lot_id = "parking 1"; //Need to set the ID of the lot
String Parking_spot_id = "one";      //Need to set the spot ID

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

Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
float init_val = 0;
String str_val = "/parkings/" + Parking_lot_id + "/spots/" + Parking_spot_id;
const char *P_Id = str_val.c_str();

void changeParkingState(bool state);
float get_avg(int samples);
float get_Mean();
bool car_detected();
void Setup_Firebase();

void setup()
{

  Setup_Firebase();

  if (!mag.begin(type))
  {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    while (1)
      ;
  }
  init_val = get_avg(50);
}

void loop()
{
  if (car_detected())
  {
    changeParkingState(true);
    delay(10000); // delay for 10s
  }
  else
  {
    changeParkingState(false);
  }
  delay(50);
}

void changeParkingState(bool state)
{
  /* 11. Try to set int data to Firebase */
  //The set function returns bool for the status of operation
  //fbdo requires for sending the data and pass as the pointer
  if (Firebase.RTDB.setBool(&fbdo, "/parkings/parking 1/spots/one", state))
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

float get_avg(int samples)
{
  sensors_event_t event;
  mag.getEvent(&event);
  float val = 0;
  float result = 0;
  unsigned int x = 0;
  for (int i = 0; i < samples; i++)
  {
    val = val + get_Mean();
    x++;
    delay(10);
  }
  result = val / x;
  return result;
}
float get_Mean()
{
  float result;
  sensors_event_t event;
  mag.getEvent(&event);
  result = sqrt(event.magnetic.x * event.magnetic.x + event.magnetic.y * event.magnetic.y + event.magnetic.z * event.magnetic.z);
  return result;
}
bool car_detected()
{
  float current_val = get_Mean();
  //Serial.println(abs((current_val - init_val))*100/init_val);
  if (abs((current_val - init_val)) * 100 / init_val > trig_TH)
  {
    bool state;
    current_val = get_avg(S);
    Serial.println(abs((current_val - init_val)) * 100 / init_val);
    if (abs((current_val - init_val)) * 100 / init_val > avg_TH)
    {
      Serial.print("Set True \n");
      state = true;
    }
    else
    {
      Serial.print("Set False \n");
      state = false;
    }
    return state;
  }
  else
  {
    return false;
  }
}