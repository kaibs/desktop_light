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
int colortemp_HA = 122;

// initial brightness
int brightness = 80;
int brightness_HA = 80;

// state change
String light_state = "OFF";

// Encoder 1
int encoder1Val = 0;
long enc1_valset = 0;
long last_button1 = 0;

// Encoder 2
int encoder2Val = 0;
long enc2_valset = 0;
long last_button2 = 0;

// var for setting initial values in HA after reboot
boolean reboot = true;

//------------- fcns encoders --------------------------

// brightness-encoder
void IRAM_ATTR isr_encoder1() {

  if (digitalRead(enc1_p2) == HIGH)
    {
      //clockwise
      if (brightness < 251){
        brightness += 4;
      } else{
        brightness = 255;
      }   
    } 
  else 
    {
      //anticlockwise
      if (brightness > 3){
        brightness -= 4;
      } else{
        brightness = 0;
      }  
    }

  enc1_valset = millis();
  Serial.println("Brightness: " + String(brightness));
}


// button brightness-encoder
void IRAM_ATTR isr_button1() {

  long cTime = millis();
  if (cTime >= (last_button1 + 50)){

    if (light_state == "OFF"){
      light_state = "ON";
    } else if (light_state == "ON"){
      light_state = "OFF";
    }
    
    char feedback[10];
    light_state.toCharArray(feedback, 10);
    client.publish("home/office/desk/deskesp/switch_mqtt", feedback);

    Serial.println("State: " + light_state);

    last_button1 = cTime;
  }
}


// colortemp-encoder
void IRAM_ATTR isr_encoder2() {

  if (digitalRead(enc2_p2) == HIGH)
    {
      //clockwise
      if (colortemp < 251){
        colortemp += 4;
      } else{
        colortemp = 255;
      }
    } 
  else 
    {
      //anticlockwise
      if (colortemp > 3){
        colortemp -= 4;
      } else{
        colortemp = 0;
      } 
    }

  enc2_valset = millis();
  Serial.println("Colortemp: " + String(colortemp));
}


// button colortemp-encoder
void IRAM_ATTR isr_button2() {

  long cTime = millis();
  if (cTime >= (last_button2 + 50))
  {
    Serial.println("BUTTON 2");
    last_button2 = cTime;
  }
}


// ----------------------------------- fcns mqtt ------------------------------------------------------

// update brightness in HA
void ha_update_brightness(){
  String helpVal = (String)brightness;
  char feedback[10];
  helpVal.toCharArray(feedback, 10);
  client.publish("home/office/desk/deskesp/brightness_mqtt", feedback);
  brightness_HA = brightness; 
}


// update colortemp in HA
void ha_update_colortemp(){
  String helpVal = (String)colortemp;
  char feedback[10];
  helpVal.toCharArray(feedback, 10);
  client.publish("home/office/desk/deskesp/colortemp_mqtt", feedback);
  colortemp_HA = colortemp;
}


// Callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
 
  if (strcmp(topic,"home/office/desk/deskesp/switch_ha")==0){
    for (int i=0;i<length;i++) {
     receivedString += (char)payload[i];
    }

    light_state = receivedString;
    Serial.println("State: " + light_state);

    char feedback[10];
    light_state.toCharArray(feedback, 10);
    client.publish("home/office/desk/deskesp/switch_mqtt", feedback);

    receivedString = "";
  }

  if (strcmp(topic,"home/office/desk/deskesp/brightness_ha")==0){
   
    for (int i=0;i<length;i++) {
     receivedString += (char)payload[i];
    }

    brightness = receivedString.toInt();
    Serial.println("Brightness: " + String(brightness));

    receivedString = "";
  }

  if (strcmp(topic,"home/office/desk/deskesp/colortemp_ha")==0){
 
    for (int i=0;i<length;i++) {
     receivedString += (char)payload[i];
    }

    colortemp = receivedString.toInt();
    Serial.println("Colortemp: " + String(colortemp));

    receivedString = "";
  }
 }

void reconnect() {

  // loop until reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");

    // attempt to connect
    if (client.connect("espDesktop", user, passw)) 
    {
      Serial.println("connected");
      // subscribe to topic
      client.subscribe("home/office/desk/deskesp/colortemp_ha");
      client.subscribe("home/office/desk/deskesp/brightness_ha");
      client.subscribe("home/office/desk/deskesp/switch_ha");
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // wait before retrying
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
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  // MQTT 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  
}

void loop() {

  // push initial values after reboot
  if ((reboot == true) && (client.connected())){
    Serial.println("update values");
    ha_update_brightness();
    ha_update_colortemp();
    reboot = false;
  }

  // check values and update HA-state
  if ((brightness != brightness_HA) && (millis() > (enc1_valset + 400))){
    ha_update_brightness();    
  }

  if ((colortemp != colortemp_HA) && (millis() > (enc2_valset + 400))){
    ha_update_colortemp();
  }


  if (!client.connected()){
   reconnect();
  }
  client.loop();
}
