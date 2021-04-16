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

// Encoder 1 (brightness)
// Button: D1 -> 5
// pin1: D6 -> 12
// pin2: D7 -> 13
const int enc1_button = 5;
const int enc1_p1 = 12;
const int enc1_p2 = 13;

// Encoder 2 (colortemp)
// pin1: D2 -> 4
// pin2: D3 -> 0
const int enc2_p1 = 4;
const int enc2_p2 = 0;

// LEDs
// D8 -> 15
// D5 -> 14
const int ww_led = 14; //warm white
const int cw_led = 15; //cold white 

//------------- intial values --------------------------

// 0 = cold, 100 = warm
int colortemp = 50;
int colortemp_HA = 50;

// initial brightness [0,100]
int brightness = 40;
int brightness_HA = 40;
int old_brightness = 40;

// state change
String light_state = "OFF";
String light_state_ha = "OFF";

// Encoder 1
int encoder1Val = 0;
long enc1_valset = 0;
long last_button1 = 0;
long button_valset = 0;

// Encoder 2
int encoder2Val = 0;
long enc2_valset = 0;
long last_button2 = 0;

// var for setting initial values in HA after reboot
boolean reboot = true;


// ------------ fcns state updates ---------------------

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


void ha_update_state(){
  char feedback[10];
  light_state.toCharArray(feedback, 10);
  client.publish("home/office/desk/deskesp/switch_mqtt", feedback);
  light_state_ha = light_state;
}

// change state if boundary crossed while dimming
void check_state(int old_bright, int new_bright){

  if (old_bright != new_bright){
  
    if ((old_bright == 0) && (new_bright > 0))
    {
      light_state = "ON";
    }

    if ((old_bright > 0) && (new_bright == 0))
    {
      light_state = "OFF";
    }
  }
}

// set MOSFET-outputs according to set vals
void set_leds() {

  // relation ww <-> cw
  int relation = colortemp - 50;

  // determine relative values for current colortemp
  int cw_val = 0;
  int ww_val = 0;
  cw_val = 50 - relation;
  ww_val = 50 + relation;

  // calc output vals according to current brightness
  int rel_cw = int(cw_val*(brightness));
  int rel_ww = int(ww_val*(brightness));

  int cw_out = map(rel_cw, 0, 10000, 0, 255);
  int ww_out = map(rel_ww, 0, 10000, 0, 255);

  // set vals
  if (light_state == "ON"){
    analogWrite(cw_led, cw_out);
    analogWrite(ww_led, ww_out);
  }
  if (light_state == "OFF"){
    analogWrite(cw_led, 0);
    analogWrite(ww_led, 0);
  }
}

//------------- fcns encoders --------------------------

// brightness-encoder
void IRAM_ATTR isr_encoder1() {

  old_brightness = brightness;

  if (digitalRead(enc1_p2) == HIGH)
    {
      //clockwise
      if (brightness < 99){
        brightness += 2;
      } else{
        brightness = 100;
      }   
    } 
  else 
    {
      //anticlockwise
      if (brightness > 4){
        brightness -= 2;
      } else{
        brightness = 0;
      }  
    }

  enc1_valset = millis();
  check_state(old_brightness, brightness);
  set_leds();
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
    
    set_leds();
    //char feedback[10];
    //light_state.toCharArray(feedback, 10);
    //client.publish("home/office/desk/deskesp/switch_mqtt", feedback);
    last_button1 = cTime;
    button_valset = millis();
  }
}


// colortemp-encoder
void IRAM_ATTR isr_encoder2() {

  if (digitalRead(enc2_p2) == HIGH)
    {
      //clockwise
      if (colortemp < 99){
        colortemp += 2;
      } else{
        colortemp = 100;
      }
    } 
  else 
    {
      //anticlockwise
      if (colortemp > 4){
        colortemp -= 2;
      } else{
        colortemp = 0;
      } 
    }

  enc2_valset = millis();
  set_leds();
}

// ----------------------------------- fcns mqtt ------------------------------------------------------

// Callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
 
  if (strcmp(topic,"home/office/desk/deskesp/switch_ha")==0){
    for (int i=0;i<length;i++) {
     receivedString += (char)payload[i];
    }

    light_state = receivedString;
    set_leds();
    ha_update_state();
    receivedString = "";
  }

  if (strcmp(topic,"home/office/desk/deskesp/brightness_ha")==0){

    old_brightness = brightness;
   
    for (int i=0;i<length;i++) {
     receivedString += (char)payload[i];
    }

    brightness = receivedString.toInt();
    check_state(old_brightness, brightness);
    set_leds();
    receivedString = "";
  }

  if (strcmp(topic,"home/office/desk/deskesp/colortemp_ha")==0){
 
    for (int i=0;i<length;i++) {
     receivedString += (char)payload[i];
    }

    colortemp = receivedString.toInt();
    set_leds();
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
  
  // MOSFETs LEDs
  pinMode(ww_led, OUTPUT); 
  pinMode(cw_led, OUTPUT); 

  // Encoder 1
  pinMode(enc1_p1, INPUT_PULLUP);
  pinMode(enc1_p2, INPUT_PULLUP);
  pinMode(enc1_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(enc1_p1), isr_encoder1, FALLING);
  attachInterrupt(digitalPinToInterrupt(enc1_button), isr_button1, RISING);

  // Encoder 2
  pinMode(enc2_p1, INPUT_PULLUP);
  pinMode(enc2_p2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(enc2_p1), isr_encoder2, FALLING);
  
  // Wifi
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
    Serial.println("update inital values");
    ha_update_brightness();
    ha_update_colortemp();
    reboot = false;
  }

  // check values and update HA-state
  if ((brightness != brightness_HA) && (millis() > (enc1_valset + 300))){
    ha_update_brightness();    
  }

  if ((colortemp != colortemp_HA) && (millis() > (enc2_valset + 300))){
    ha_update_colortemp();
  }

  if ((light_state != light_state_ha) && (millis() > (button_valset + 300))){
    ha_update_state();
  }

  // check mqtt connection
  if (!client.connected()){
   reconnect();
  }
  client.loop();
}
