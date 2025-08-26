#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "pitches.h"

// ---------- WiFi ----------
const char* ssid = "<ssid>";
const char* password = "<password>";

// ---------- DHT22 ----------
#define DHTPIN A0
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ---------- OLED ----------
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------- NTP ----------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

// ---------- LEDs ----------
const uint8_t LED_RED_RIGHT = 8;
const uint8_t LED_GREEN_RIGHT = 9;
const uint8_t LED_RED_LEFT = TX;
const uint8_t LED_GREEN_LEFT = RX;
const uint8_t NUM_LEDS = 4;
const uint8_t ledPins[NUM_LEDS] =
    { LED_RED_RIGHT, LED_GREEN_RIGHT, LED_RED_LEFT, LED_GREEN_LEFT };

// ---------- Web server ----------
WebServer server(80);

// ---------- Buttons ----------
#define BUTTON1_PIN A1
#define BUTTON2_PIN A2
#define BUTTON3_PIN A3

int prevState1 = LOW, prevState2 = LOW, prevState3 = LOW;
unsigned long btn2PressStartMillis = 0;
bool btn2WasLong = false;

// ---------- Modes & Timer ----------
uint8_t currentMode = 1;  // 1=Clock, 2=Timer
uint8_t timerUiState = 0;
unsigned int timerSettingMinutes = 40;
bool timerRunning = false;
unsigned long timerStartMillis = 0;
unsigned long timerDurationMillis = 0;

// ---------- Alert ----------
bool timerAlertActive = false;

// ---------- Buzzer ----------
const int buzzerPin = 1;

// LG melody end alert
int melody[] = { NOTE_E5, NOTE_G5, NOTE_B5, NOTE_C6,
                 NOTE_B5, NOTE_G5, NOTE_E5, NOTE_G5,
                 NOTE_B5, NOTE_A5, NOTE_G5 };
int noteDurations[] = { 8, 8, 4, 4,
                        8, 8, 4, 8,
                        8, 8, 2 };
int melodyLen = sizeof(melody) / sizeof(int);
int melodyIndex = 0, melodyRepeatCount = 0;
unsigned long lastNoteTime = 0;
int currentNoteDuration = 0;
bool noteActive = false;

// Calling melody during pause
int callMelody[] = { NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5 };
int callDurations[] = { 8, 8, 8, 8 };
int callMelodyLen = sizeof(callMelody) / sizeof(int);
int callMelodyIndex = 0;
unsigned long callNoteEndTime = 0;
bool callNoteActive = false;
unsigned long lastCallTrigger = 0;
const unsigned long callInterval = 10000;

// ---------- Ultrasonic ----------
#define TRIG_PIN 7
#define ECHO_PIN 6
const long proximityThresholdCM = 50;
bool timerPaused = false;
unsigned long pauseStartMillis = 0;
unsigned long pausedAccumulated = 0;

void handleRoot();
void handleSet();
void handleData();
void handleButtons();
long readUltrasonicCM();
void handleTimerAlert();
void handleCallingSound();
void breatheLED();
void blinkLEDsFast();

void setup() {
  Serial.begin(115200);
  dht.begin();
  u8g2.begin();
  for(uint8_t i=0;i<NUM_LEDS;i++){ pinMode(ledPins[i],OUTPUT); digitalWrite(ledPins[i],HIGH);}
  pinMode(BUTTON1_PIN,INPUT_PULLDOWN);
  pinMode(BUTTON2_PIN,INPUT_PULLDOWN);
  pinMode(BUTTON3_PIN,INPUT_PULLDOWN);
  pinMode(buzzerPin,OUTPUT);
  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);

  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");}
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  server.on("/",HTTP_GET,handleRoot);
  server.on("/set",HTTP_POST,handleSet);
  server.on("/data",HTTP_GET,handleData);
  server.begin();
}

void loop(){
  server.handleClient();

  if(timerAlertActive){ handleTimerAlert(); return; }

  handleButtons();
  timeClient.update();
  breatheLED();

  // Ultrasonic pause check
  if(currentMode==2 && timerUiState==1){
    long dist=readUltrasonicCM();
    if(dist>proximityThresholdCM || dist<0){
      if(!timerPaused && timerRunning){
        timerPaused=true;
        pauseStartMillis=millis();
        timerRunning=false;
        lastCallTrigger=millis();
      }
    }else if(timerPaused){
      pausedAccumulated+=millis()-pauseStartMillis;
      timerStartMillis+=millis()-pauseStartMillis;
      timerPaused=false;
      timerRunning=true;
      noTone(buzzerPin);
    }
  }

  handleCallingSound();

  u8g2.clearBuffer();
  float t=dht.readTemperature(), h=dht.readHumidity();
  if(currentMode==1){ // clock
    u8g2.setFont(u8g2_font_7x13_tr);
    if(!isnan(t)&&!isnan(h)){
      char ts[16],hs[16];
      snprintf(ts,sizeof(ts),"T: %.1fC",t);
      snprintf(hs,sizeof(hs),"H: %.1f%%",h);
      u8g2.drawStr(0,12,ts);
      u8g2.drawStr(70,12,hs);
    }
    unsigned long epoch=timeClient.getEpochTime();
    int hr=(epoch%86400L)/3600, mi=(epoch%3600)/60, se=epoch%60;
    char timeStr[16]; snprintf(timeStr,sizeof(timeStr),"%02d:%02d:%02d",hr,mi,se);
    u8g2.setFont(u8g2_font_fur20_tr);
    u8g2.drawStr((128-u8g2.getStrWidth(timeStr))/2,52,timeStr);
  }else if(currentMode==2){
    u8g2.setFont(u8g2_font_6x13_tr);
    if(!isnan(t)&&!isnan(h)){
      char ts[16],hs[16];
      snprintf(ts,sizeof(ts),"T: %.1fC",t);
      snprintf(hs,sizeof(hs),"H: %.1f%%",h);
      u8g2.drawStr(0,12,ts);
      u8g2.drawStr(70,12,hs);
    }
    if(timerUiState==0){
      char buf[16];
      u8g2.setFont(u8g2_font_fur17_tr);
      snprintf(buf,sizeof(buf),"%3d min",timerSettingMinutes);
      u8g2.drawStr((128-u8g2.getStrWidth(buf))/2,38,buf);
    }else{
      if(timerPaused){
        unsigned long elapsed=pauseStartMillis-timerStartMillis;
        unsigned long rem=(timerDurationMillis>elapsed)?(timerDurationMillis-elapsed)/1000:0;
        char remStr[16]; snprintf(remStr,sizeof(remStr),"%02lu:%02lu",rem/60,rem%60);
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr((128-u8g2.getStrWidth("PAUSED"))/2,40,"PAUSED");
        u8g2.drawStr((128-u8g2.getStrWidth(remStr))/2,62,remStr);
      }else{
        unsigned long elapsed=millis()-timerStartMillis;
        if(elapsed>=timerDurationMillis){
          timerRunning=false;
          timerAlertActive=true;
          melodyIndex=0; melodyRepeatCount=0; noteActive=false;
        }else{
          unsigned long rem=(timerDurationMillis-elapsed)/1000;
          char bigStr[16]; snprintf(bigStr,sizeof(bigStr),"%02lu:%02lu",rem/60,rem%60);
          u8g2.setFont(u8g2_font_fur20_tr);
          u8g2.drawStr((128-u8g2.getStrWidth(bigStr))/2,48,bigStr);
        }
      }
    }
  }
  u8g2.sendBuffer();
}

void handleRoot(){
  String html="<!DOCTYPE html><html><head><title>ESP32 Timer Control</title>";
  html+="<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html+="<style>body{font-family:Arial;text-align:center;padding:10px;}input,select,button{font-size:18px;padding:5px;margin:5px;}</style></head><body>";
  html+="<h2>ESP32 Timer & Mode Control</h2><form action='/set' method='POST'>";
  html+="<label>Mode:</label><select name='mode'>";
  html+="<option value='1'"+String(currentMode==1?" selected":"")+">Clock</option>";
  html+="<option value='2'"+String(currentMode==2?" selected":"")+">Timer</option></select><br>";
  html+="<label>Timer (minutes):</label><input type='number' name='timer' min='1' max='240' value='"+String(timerSettingMinutes)+"'><br>";
  html+="<button type='submit' name='apply' value='1'>Apply</button>";
  if(currentMode==2 && timerUiState==0){
    html+="<button type='submit' name='start' value='1'>Start Timer</button>";
  }
  html+="</form></body></html>";
  server.send(200,"text/html",html);
}

void handleSet(){
  if(server.hasArg("mode")){
    int m=server.arg("mode").toInt();
    if(m==1||m==2){ currentMode=m; timerUiState=0; timerRunning=false; timerAlertActive=false; }
  }
  if(server.hasArg("timer")){
    int tval=server.arg("timer").toInt();
    if(tval>=1 && tval<=240) timerSettingMinutes=tval;
  }
  if(server.hasArg("start") && currentMode==2 && timerUiState==0){
    timerDurationMillis=timerSettingMinutes*60000UL;
    timerStartMillis=millis();
    pausedAccumulated=0;
    timerRunning=true; timerUiState=1;
    Serial.println("Timer started via Web UI");
  }
  server.sendHeader("Location","/");
  server.send(303);
}

void handleData(){
  unsigned long epoch=timeClient.getEpochTime();
  int hr=(epoch%86400L)/3600, mi=(epoch%3600)/60, se=epoch%60;
  float t=dht.readTemperature(), h=dht.readHumidity();
  long dist=readUltrasonicCM();
  String timerState="idle";
  if(timerAlertActive) timerState="alert"; else if(timerPaused) timerState="paused"; else if(timerRunning) timerState="running";
  unsigned long elapsed=0;
  if(timerPaused) elapsed=pauseStartMillis-timerStartMillis;
  else if(timerRunning) elapsed=millis()-timerStartMillis;
  unsigned long rem=(timerDurationMillis>elapsed)?(timerDurationMillis-elapsed)/1000:0;
  unsigned long uptimeSec=millis()/1000;
  String json="{\"temperature\":"+String(isnan(t)?0:t)+",\"humidity\":"+String(isnan(h)?0:h)
    +",\"distance_cm\":"+String(dist)+",\"timer_state\":\""+timerState+"\""
    +",\"timer_remaining_sec\":"+String(rem)+",\"alert_repeats\":"+String(melodyRepeatCount)
    +",\"uptime_sec\":"+String(uptimeSec)+",\"time\":\""+String(hr)+":"+String(mi)+":"+String(se)+"\"}";
  server.send(200,"application/json",json);
}

void handleButtons(){
  int s1=digitalRead(BUTTON1_PIN), s2=digitalRead(BUTTON2_PIN), s3=digitalRead(BUTTON3_PIN);
  static unsigned long lastDeb=0; unsigned long now=millis();
  if(now-lastDeb<120) goto saveStates;
  if(s2==HIGH && prevState2==LOW){ btn2PressStartMillis=now; btn2WasLong=false; }
  if(s2==HIGH && prevState2==HIGH && currentMode==2 && timerUiState==0 && (now-btn2PressStartMillis>1000) && !btn2WasLong){
    timerDurationMillis=timerSettingMinutes*60000UL; timerStartMillis=millis(); pausedAccumulated=0; timerRunning=true; timerUiState=1;
    btn2WasLong=true; lastDeb=now;
  }
  if(s2==LOW && prevState2==HIGH && currentMode==2 && timerUiState==0 && !btn2WasLong){
    timerSettingMinutes++; if(timerSettingMinutes>240) timerSettingMinutes=1; lastDeb=now;
  }
  if(s3==HIGH && prevState3==LOW){
    if(currentMode==2 && timerUiState==0){
      if(timerSettingMinutes==1) timerSettingMinutes=240; else timerSettingMinutes--; lastDeb=now;
    } else if(currentMode==2 && timerUiState==1){
      timerRunning=false; timerUiState=0; currentMode=1; lastDeb=now;
    }
  }
  if(currentMode==1 && s2==HIGH && prevState2==LOW){
    currentMode=2; timerUiState=0; lastDeb=now;
  }
saveStates: prevState1=s1; prevState2=s2; prevState3=s3;
}

long readUltrasonicCM(){
  digitalWrite(TRIG_PIN,LOW); delayMicroseconds(2); digitalWrite(TRIG_PIN,HIGH); delayMicroseconds(10); digitalWrite(TRIG_PIN,LOW);
  long d=pulseIn(ECHO_PIN,HIGH,30000UL); if(d==0) return -1; return d*0.0343/2;
}

void handleTimerAlert(){
  unsigned long now=millis();

  if(!noteActive && now>=lastNoteTime){
    int n=melody[melodyIndex]; currentNoteDuration=1000/noteDurations[melodyIndex];
    tone(buzzerPin,n,currentNoteDuration);
    blinkLEDsFast();
    bool flashOn = (now/300)%2;
    u8g2.clearBuffer(); u8g2.setFont(u8g2_font_fur20_tr);
    if(flashOn){
      const char* msg="TIME UP!"; u8g2.drawStr((128-u8g2.getStrWidth(msg))/2,42,msg);
    }
    u8g2.sendBuffer();
    lastNoteTime=now+currentNoteDuration; noteActive=true;
  }else if(noteActive && now>=lastNoteTime){
    noTone(buzzerPin);
    for(uint8_t i=0;i<NUM_LEDS;i++) digitalWrite(ledPins[i],HIGH);
    lastNoteTime=now+currentNoteDuration*0.3; melodyIndex++;
    if(melodyIndex>=melodyLen){ melodyIndex=0; melodyRepeatCount++; }
    noteActive=false;
  }
  if(melodyRepeatCount>=3){
    timerAlertActive=false; melodyRepeatCount=0; melodyIndex=0;
    noTone(buzzerPin);
    for(uint8_t i=0;i<NUM_LEDS;i++) digitalWrite(ledPins[i],HIGH);
    u8g2.clearBuffer(); u8g2.sendBuffer();
    currentMode=1; timerUiState=0;
  }
}

void handleCallingSound(){
  unsigned long now=millis();
  if(!timerPaused){
    if(callNoteActive){ noTone(buzzerPin); callNoteActive=false; callMelodyIndex=0;}
    return;
  }
  if(now-lastCallTrigger>=callInterval){
    if(!callNoteActive || now>=callNoteEndTime){
      if(callNoteActive) noTone(buzzerPin);
      int n=callMelody[callMelodyIndex]; int d=1000/callDurations[callMelodyIndex];
      tone(buzzerPin,n,d); callNoteEndTime=now+d; callNoteActive=true;
      blinkLEDsFast();
      callMelodyIndex++; if(callMelodyIndex>=callMelodyLen){ callMelodyIndex=0; lastCallTrigger=now; }
    }else{
      blinkLEDsFast();
    }
  }else if(!timerAlertActive){
    for(uint8_t i=0;i<NUM_LEDS;i++) digitalWrite(ledPins[i],HIGH);
  }
}

void breatheLED(){
  if(timerAlertActive) return;
  static int step=0; static unsigned long last=0; const int steps=100; const unsigned long interval=5;
  unsigned long now=millis(); if(now-last<interval) return; last=now;
  float pi=3.14159265; float bf=(1-cos(pi*(float)step/steps))/2.0; int b=(int)(bf*255); uint8_t pwm=255-b;
  static uint8_t ledIndex=0;
  for(uint8_t i=0;i<NUM_LEDS;i++){ if(i==ledIndex) analogWrite(ledPins[i],pwm); else digitalWrite(ledPins[i],HIGH);}
  step++; if(step>steps){ step=0; ledIndex=(ledIndex+1)%NUM_LEDS; }
}

void blinkLEDsFast(){
  static unsigned long last=0; static bool s=false; unsigned long now=millis();
  if(now-last>100){ last=now; s=!s; for(uint8_t i=0;i<NUM_LEDS;i++){ digitalWrite(ledPins[i], s?LOW:HIGH);} }
}

