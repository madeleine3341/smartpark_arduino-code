#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <ressources.h>

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

void setup()
{

  Serial.begin(115200);

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

  /* 7. Assign the project host and api key (required) */
  config.host = FIREBASE_HOST;
  config.api_key = API_KEY;

  /* 8. Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* 9. Initialize the library with the autentication data */
  Firebase.begin(&config, &auth);

  /* 10. Enable auto reconnect the WiFi when connection lost */
  Firebase.reconnectWiFi(true);
}

void loop()
{
  changeParkingState(true);
}

void changeParkingState(bool state)
{
  /* 11. Try to set int data to Firebase */
  //The set function returns bool for the status of operation
  //fbdo requires for sending the data and pass as the pointer
  if (Firebase.RTDB.setBool(&fbdo, "/parkings/parking 1/status", state))
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