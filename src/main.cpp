#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// credentials
#include "credentials.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER_IP;
const char* user= MQTT_USER;
const char* passw= MQTT_PASSWORD;

// Wifi
WiFiClient espDesktop;

//MQTT
PubSubClient client(espDesktop);
String receivedString;

//------------ pin config -----------------------------

// Encoder 1
// Button: D5 -> 14
// pin1: D6 -> 12
// pin2: D7 -> 13
const int enc1_button = 14;
const int enc1_p1 = 12;
const int enc1_p2 = 13;

// Encoder 2
// Button: D1 -> 5
// pin1: D2 -> 4
// pin2: D3 -> 0
const int enc2_button = 5;
const int enc2_p1 = 4;
const int enc2_p2 = 0;

// LED
const int led = 5;

//------------- intial values --------------------------

// 0 = cold, 255 = warm
int colortemp = 122;
int colortemp_HA = 0;

// initial brightness
int brightness = 122;
int brightness_HA = 0;


//Encoder 1
int encoder1Val = 0;
long enc1_valset = 0;
long last_button1 = 0;

//Encoder 2
int encoder2Val = 0;
long enc2_valset = 0;
long last_button2 = 0;

//------------- fcns encoders --------------------------

void IRAM_ATTR isr_encoder1() {

  if (digitalRead(enc1_p2) == HIGH)
    {
      //clockwise
      brightness += 1;
    } else if (brightness > 0) {
      //anticlockwise
      brightness -= 1;
    }

  enc1_valset = millis();
  Serial.println("Brightness: " + String(brightness));
}

void IRAM_ATTR isr_button1() {

  long cTime = millis();
  if (cTime >= (last_button1 + 50)){

    Serial.println("BUTTON 1");
    last_button1 = cTime;

  }

}

void IRAM_ATTR isr_encoder2() {

  if (digitalRead(enc2_p2) == HIGH)
    {
      //clockwise
      colortemp += 1;
    } else if (colortemp > 0) {
      //anticlockwise
      colortemp -= 1;
    }

  enc2_valset = millis();
  Serial.println("Colortemp: " + String(colortemp));
}

void IRAM_ATTR isr_button2() {

  long cTime = millis();
  if (cTime >= (last_button2 + 50)){

    Serial.println("BUTTON 2");
    last_button2 = cTime;

  }

}


// ----------------------------------- fcns mqtt ------------------------------------------------------


// FCNs LED-Strip
void stripON(){

  analogWrite(led, brightness);
  String helpVal = (String)brightness;
  char feedback[10];
  helpVal.toCharArray(feedback, 10);
  client.publish("home/office/desk/deskesp/feedback", feedback);
}

void stripOFF(){
  
  analogWrite(led, 0);
  char feedback[5] = "0";
  client.publish("home/office/desk/deskesp/feedback", feedback);
}

void setBright(){
  
  analogWrite(led, brightness);
  String helpVal = (String)brightness;
  char feedback[10];
  helpVal.toCharArray(feedback, 10);
  client.publish("home/office/desk/deskesp/feedback", feedback);
}

// Callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
 
 if (strcmp(topic,"home/office/desk/deskesp/switch")==0){
  for (int i=0;i<length;i++) {
   receivedString += (char)payload[i];


   if (receivedString == "ON"){
     stripON();
   }

   if (receivedString == "OFF"){
     stripOFF();
   }
  }
  receivedString = "";
 }

 if (strcmp(topic,"home/office/desk/deskesp/brightness")==0){
 
  for (int i=0;i<length;i++) {
   receivedString += (char)payload[i];
  }
  
  brightness = receivedString.toInt();
  setBright();
  
  receivedString = "";
  }
 }

void reconnect() {

 // Loop until we're reconnected
 while (!client.connected()) {
 Serial.print("Attempting MQTT connection...");

 // Attempt to connect
 if (client.connect("espDesktop", user, passw)) {
  Serial.println("connected");
  // ... and subscribe to topic
  client.subscribe("home/office/desk/deskesp/switch");
  client.subscribe("home/office/desk/deskesp/brightness");
  
 } else {
  Serial.print("failed, rc=");
  Serial.print(client.state());
  Serial.println(" try again in 5 seconds");
  // Wait 5 seconds before retrying
  delay(5000);
  }
 }
}


void setup() {

  Serial.begin(9600);
  
  // MOSFET LED
  pinMode(led, OUTPUT); 

  // Encoder 1
  pinMode(enc1_p1, INPUT_PULLUP);
  pinMode(enc1_p2, INPUT_PULLUP);
  pinMode(enc1_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(enc1_p1), isr_encoder1, FALLING);
  attachInterrupt(digitalPinToInterrupt(enc1_button), isr_button1, RISING);

  // Encoder 2
  pinMode(enc2_p1, INPUT_PULLUP);
  pinMode(enc2_p2, INPUT_PULLUP);
  pinMode(enc2_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(enc2_p1), isr_encoder2, FALLING);
  attachInterrupt(digitalPinToInterrupt(enc2_button), isr_button2, RISING);
  
  //Wifi
  WiFi.hostname("espDesktop");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  // MQTT 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

void loop() {

  // check values and update HA-state
  if ((brightness != brightness_HA) && (millis() > (enc1_valset + 400))){

    String helpVal = (String)brightness;
    char feedback[10];
    helpVal.toCharArray(feedback, 10);
    client.publish("home/office/desk/deskesp/brightness_val", feedback);
    brightness_HA = brightness;
  }

  if ((colortemp != colortemp_HA) && (millis() > (enc2_valset + 400))){

    String helpVal = (String)colortemp;
    char feedback[10];
    helpVal.toCharArray(feedback, 10);
    client.publish("home/office/desk/deskesp/colortemp_val", feedback);
    colortemp_HA = colortemp;
  }

  
  



  if (!client.connected()){
   reconnect();
  }
  client.loop();

}
