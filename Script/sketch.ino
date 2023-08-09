#include <PubSubClient.h>
#include <WiFi.h>
#include "DHTesp.h"
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

#define LED_BUILTIN 2
#define NTP_SERVER     "pool.ntp.org"

const int DHT_PIN = 15;
const int LDR_PIN = 34;
const int Servo_PIN = 18;
const int Buzzer_PIN = 12;
int buzzer_delay = 500;
int buzzer_frequency = 262;
int buzzer_configuration = 1;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;
Servo servo;
LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 20, 4);

char tempAr[6];
char humAr[6];
char ldrAr[6];

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(Servo_PIN, 500, 2400);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(Buzzer_PIN, OUTPUT);

  LCD.init();
  LCD.backlight();
  LCD.setCursor(5, 1);
  LCD.print("Welcome to");
  LCD.setCursor(6, 2);
  LCD.print("+Medibox");
  configTime(5 * 3600, 30 * 60, NTP_SERVER);
  delay(1000);
  LCD.clear();
}

void loop() {
  if (!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();

  updateTemperature();
  Serial.println("Temperature - " + String(tempAr));
  mqttClient.publish("Temperature-190586H", tempAr);
  updateHumidity();
  Serial.println("Humidity - " + String(humAr));
  mqttClient.publish("Humidity-190586H", humAr);
  updateLightIntensity();
  Serial.println("Light Intensity - " + String(ldrAr));
  mqttClient.publish("LightIntensity-190586H", ldrAr);
  printTime();
  delay(200);
}

void setupWifi(){
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void connectToBroker(){
  while (!mqttClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-12345678")){
      Serial.println("connected");
      mqttClient.subscribe("MAIN-ON-OFF-190586H");
      mqttClient.subscribe("Angle-190586H");
      mqttClient.subscribe("Time-190586H");
      mqttClient.subscribe("Buzzer-Delay-190586H");
      mqttClient.subscribe("Buzzer-Frequency-190586H");
      mqttClient.subscribe("Buzzer-Configuration-190586H");
      mqttClient.subscribe("Alarm-190586H");
    }else {
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
}

void updateHumidity(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.humidity, 2).toCharArray(humAr, 6);
}

void updateLightIntensity(){
  float ldrval = (float)analogRead(LDR_PIN) / 4095.0;
  String(ldrval, 2).toCharArray(ldrAr, 6);
}

void receiveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");

  char payloadCharAr[length];
  for (int i = 0; i<length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println();

  controlServo(topic, payloadCharAr);
  controlBuzzer(topic, payloadCharAr);
  alarmsForMedications(topic, payloadCharAr);
}

void controlServo(char* topic, char payloadCharAr[]) {
  if (strcmp(topic, "Angle-190586H") == 0) {
    if (payloadCharAr[0] =='NaN'){
      int angle = 30 + (180 - 30) * (1 - (float)analogRead(LDR_PIN) / 4095.0) * 0.75;
      servo.write(angle);
    }else{
      int angle = atoi(payloadCharAr);
      servo.write(angle);
    }
  }
}

void controlBuzzer(char* topic, char payloadCharAr[]) {
  if (strcmp(topic, "Buzzer-Delay-190586H") == 0){
    buzzer_delay = atoi(payloadCharAr);
  }

  if (strcmp(topic, "Buzzer-Frequency-190586H") == 0){
    buzzer_frequency = atoi(payloadCharAr);
  }

  if (strcmp(topic, "Buzzer-Configuration-190586H") == 0){
    buzzer_configuration = atoi(payloadCharAr);
  }

  if (strcmp(topic, "MAIN-ON-OFF-190586H") == 0) {
    if (payloadCharAr[0] =='1'){
      tone(Buzzer_PIN, 262, 250);
      noTone(Buzzer_PIN);
    }
  }
}

void alarmsForMedications(char* topic, char payloadCharAr[]){
  if (strcmp(topic, "Alarm-190586H") == 0){
    if (payloadCharAr[0] =='1'){
      LCD.setCursor(2, 2);
      LCD.println("Please take your");
      LCD.setCursor(4, 3);
      LCD.println("medications!");
      if (buzzer_configuration == 1){
        tone(Buzzer_PIN, buzzer_frequency, 10000);
      }else{
        for (int j = 0; j < 10; j++){
          tone(Buzzer_PIN, buzzer_frequency, buzzer_delay);
          delay(250);
        }
      }
    }
    LCD.clear();
  }
}

void printTime(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    LCD.setCursor(0, 1);
    LCD.println("Connection Err");
    return;
  }
  LCD.setCursor(0, 0);
  LCD.println(&timeinfo, "%H:%M:%S");

  LCD.setCursor(0, 1);
  LCD.println(&timeinfo, "%d/%m/%Y");
}
