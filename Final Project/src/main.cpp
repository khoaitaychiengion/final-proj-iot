#include <Arduino.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include "DHTesp.h"

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *mqttServer = "broker.mqtt-dashboard.com";
int port = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

int SOLID_MOISTURE_PIN = 34;
int DHT_PIN = 15;
int WATER_PUMP_PIN = 25;
DHTesp dhtSensor;

void wifiConnect()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

void mqttConnect()
{
  while (!mqttClient.connected())
  {
    Serial.println("Attemping MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("connected");

      //***Subscribe all topic you need***
    }

    else
    {
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

// MQTT Receiver
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");

  String message = "";
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  Serial.println(message);
  if (String(topic) == "esp/control")
  {
    if (message.startsWith("WATER"))
    {
      // Parse command
      int intensity = 0; // From 0-100 (%)
      int duration = 0;
      sscanf(message.c_str(), "WATER %d %d", &intensity, &duration);

      // Map intensity to PWM (Pulse Width Modulation) value (0-255)
      int pwmValue = map(intensity, 0, 100, 0, 255);

      analogWrite(WATER_PUMP_PIN, pwmValue);
      Serial.print("Water pump intensity set to: ");
      Serial.println(intensity);

      // Duration
      if (duration > 0)
      {
        delay(duration * 1000);
        analogWrite(WATER_PUMP_PIN, 0); // Turn off pump after duration
        Serial.println("Water pump turned OFF after duration");
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(SOLID_MOISTURE_PIN, INPUT);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  Serial.print("Connecting to wifi");
  wifiConnect();

  mqttClient.setServer(mqttServer, port);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive(90);
}

void loop()
{
  if (!mqttClient.connected())
  {
    mqttConnect();
  }
  mqttClient.loop();

  // TempAndHumidity data = dhtSensor.getTempAndHumidity();
  // Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  // Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  // Serial.println("---");
  // int temp = random(0, 100);
  // char buffer[50];
  // sprintf(buffer, "%d", temp);
  // mqttClient.publish("randomNumber", buffer);

  int solidMoisture = analogRead(SOLID_MOISTURE_PIN);
  char buffer[10];
  sprintf(buffer, "%d", solidMoisture);

  TempAndHumidity tempAndHumidData = dhtSensor.getTempAndHumidity();
  char buffer1[10];
  sprintf(buffer1, "%.1f", tempAndHumidData.temperature);
  char buffer2[10];
  sprintf(buffer2, "%.2f", tempAndHumidData.humidity);

  mqttClient.publish("dataFromESP32/solidMoisture", buffer);
  mqttClient.publish("dataFromESP32/temperature", buffer1);
  mqttClient.publish("dataFromESP32/humidity", buffer2);

  delay(1000);
}
