#include <Arduino.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include "DHTesp.h"
#include <FirebaseESP32.h>
#include <ESP32Servo.h>
#include <time.h>

// config wifi
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";

// config mqtt server
const char* MQTT_SERVER = "broker.mqtt-dashboard.com";
int port = 1883;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// config firebase
const char* FIREBASE_HOST = "https://final-project-18389-default-rtdb.asia-southeast1.firebasedatabase.app/";
const char* FIREBASE_AUTH = "tKxIGa49xu4lckjgDd8nPVpeWIu34AgOyqGLwaMY";
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// soli moisture sensor
int SOLID_MOISTURE_PIN = 34;

// moisture and temperature sensor
int DHT_PIN = 15;
DHTesp dhtSensor;

// servo for controll watering
int SERVO_PIN = 18;
Servo servo;
int CLOSE_WATER = 0;
int OPEN_WATER = 90;

int WATER_LEVEL_PIN = 17;
int LIGHT_SENSOR_PIN = 32;
int LED_PIN = 4;

unsigned long int lastTimeSendData = 0;

// connect wifi
void wifiConnect() {
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

// connect mqtt server
void mqttConnect() {
  while (!mqttClient.connected()) {
    Serial.println("Attemping MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");

      //***Subscribe all topic you need***
      mqttClient.subscribe("data/dataFromWeb");
    }

    else {
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

// MQTT Receiver
void callback(char* topic, byte* message, unsigned int length) {
  Serial.println(topic);
  String strMsg;
  for(int i=0; i<length; i++) {
    strMsg += (char)message[i];
  }
  Serial.println(strMsg);

  //***Code here to process the received package***
  if (strMsg == "water") {
    servo.write(OPEN_WATER);
    delay(4000);
    servo.write(CLOSE_WATER);
  }

  else if (strMsg == "led") {
    digitalWrite(LED_PIN, HIGH);
  }
}

void saveToFireBase(char timePath[], String topic, char buffer[]) {
  char fullPath[40];
  sprintf(fullPath, "%s/%s", timePath, topic);
  String msg = buffer;

  if (Firebase.setString(firebaseData, fullPath, msg)) {
      Serial.print(topic);
      Serial.print(" sent to Firebase! - Msg: ");
      Serial.println(msg);
  } else {
      Serial.print("Firebase error: " );
      Serial.println(firebaseData.errorReason());
  }
}

void setup() {
  Serial.begin(115200);

  // connect wifi
  Serial.print("Connecting to wifi");
  wifiConnect();

  // connect mqtt server
  mqttClient.setServer(MQTT_SERVER, port);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive( 90 );

  // connect firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  // test Firebase connection
  if (Firebase.setInt(firebaseData, "/Test", 123))
    Serial.println("Firebase connected!");
  else {
    Serial.println("Failed to connect to Firebase.");
    Serial.println(firebaseData.errorReason());
  }

  pinMode(SOLID_MOISTURE_PIN, INPUT);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(SERVO_PIN);
  servo.write(CLOSE_WATER);
  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // get current time from ntp server
  configTime(25200, 0, "time.google.com");
}

void loop() {
  // connect mqtt server
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();

  if (millis() - lastTimeSendData >= 5000 || lastTimeSendData == 0) {
    lastTimeSendData = millis();
    
    // get time data to store in database
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }

    int year = timeinfo.tm_year + 1900;
    int month = timeinfo.tm_mon + 1;
    int day = timeinfo.tm_mday;
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int second = timeinfo.tm_sec;

    char timePath[25];
    snprintf(timePath, sizeof(timePath), "data/%02d-%02d-%04d/%02d:%02d:%02d", day, month, year, hour, minute, second);

    // read and send data to database

    // soil moisture sensor: read data, save data to database, send data to web
    int solidMoisture = analogRead(SOLID_MOISTURE_PIN);
    int solidMoisturePercentage = map(solidMoisture, 0, 4095, 0, 100);
    char buffer[20];
    sprintf(buffer, "%d%%", solidMoisturePercentage);
    saveToFireBase(timePath, "SoilMoisture", buffer);
    mqttClient.publish("data/dataFromESP32/solidMoisture", buffer);

    // DHT22 sensor: read data, save data to database, send data to web
    TempAndHumidity tempAndHumidData = dhtSensor.getTempAndHumidity();
    sprintf(buffer, "%.1f°C", tempAndHumidData.temperature);
    saveToFireBase(timePath, "Temperature", buffer);
    mqttClient.publish("data/dataFromESP32/temperature", buffer);
    sprintf(buffer, "%.2f%%", tempAndHumidData.humidity);
    saveToFireBase(timePath, "Humidity", buffer); 
    mqttClient.publish("data/dataFromESP32/humidity", buffer);

    // read and save data of water level sensor
    int levelOfWater = digitalRead(WATER_LEVEL_PIN);
    if (levelOfWater == HIGH) sprintf(buffer, "Run out of water");
    else {
      digitalWrite(LED_PIN, LOW);
      sprintf(buffer, "Full of water");
    }
    saveToFireBase(timePath, "WaterLevel", buffer);
    mqttClient.publish("data/dataFromESP32/waterlevel", buffer);

    // read and save data of light sensor
    int lightIntensity = analogRead(LIGHT_SENSOR_PIN);
    if (lightIntensity < 1300) {
      sprintf(buffer, "Lack of brightness");
    } else if (lightIntensity < 2600) {
      sprintf(buffer, "Appropriate brightness");
    } else {
      sprintf(buffer, "Too much brightness");
    }
    saveToFireBase(timePath, "Light", buffer); 
    mqttClient.publish("data/dataFromESP32/light", buffer);
  }

  //delay(5000);
}
