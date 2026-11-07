#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PZEM004Tv30.h>
#include <ArduinoJson.h>
#include <math.h>

// Update WiFi and server address for your network.
const char* WIFI_SSID="Your WiFi name";
const char* WIFI_PASSWORD="Your WiFi password";
const char* SERVER="Your IP address";
const char* GSM_PHONE="Your phone number";
constexpr int PZEM_RX=16,PZEM_TX=17,GSM_RX=22,GSM_TX=23;
constexpr int BUZZER_PIN=33;
constexpr int COUNT=3;
const char* DEVICE_NAMES[COUNT]={"Iron Box","Kettle","Hair Dryer"};
constexpr int RELAY_PINS[COUNT]={18,19,21};
constexpr uint8_t PZEM_ADDRESSES[COUNT]={0x57,0x56,0x55};
constexpr float WARNING_KWH[COUNT]={10,10,10};
constexpr float SECOND_WARNING_KWH[COUNT]={12,12,0.90};
constexpr int RELAY_ON=LOW,RELAY_OFF=HIGH;
constexpr unsigned long READ_MS=2000,POST_MS=5000,COMMAND_MS=1000;
constexpr unsigned long CALL_DURATION_MS=15000;
constexpr unsigned long TRIP_DURATION_MS=60000;
PZEM004Tv30 pzem1(Serial2,PZEM_RX,PZEM_TX,PZEM_ADDRESSES[0]);
PZEM004Tv30 pzem2(Serial2,PZEM_RX,PZEM_TX,PZEM_ADDRESSES[1]);
PZEM004Tv30 pzem3(Serial2,PZEM_RX,PZEM_TX,PZEM_ADDRESSES[2]);
PZEM004Tv30* meters[COUNT]={&pzem1,&pzem2,&pzem3};
struct Room {float voltage=0,current=0,power=0,energy=0,frequency=0,pf=0;bool communication=false,energyValid=false,tripped=false,pending=false;uint8_t stage=0;unsigned long tripStarted=0;};
Room rooms[COUNT];
unsigned long lastRead=0,lastPost=0,lastCommand=0,buzzerUntil=0;
void buzzer(){digitalWrite(BUZZER_PIN,HIGH);buzzerUntil=millis()+1000;}
void maintainRelays(){for(int i=0;i<COUNT;i++)digitalWrite(RELAY_PINS[i],rooms[i].tripped?RELAY_OFF:RELAY_ON);}
void readRoom(int i){
 Room &r=rooms[i];PZEM004Tv30 &p=*meters[i];
 float v=p.voltage();if(isnan(v)){r.communication=false;r.energyValid=false;Serial.printf("%s PZEM error\n",DEVICE_NAMES[i]);return;}
 r.communication=true;r.voltage=v;
 float x=p.current();if(!isnan(x))r.current=x;
 x=p.power();if(!isnan(x))r.power=x;
 x=p.energy();r.energyValid=!isnan(x);if(r.energyValid)r.energy=x;
 x=p.frequency();if(!isnan(x))r.frequency=x;
 x=p.pf();if(!isnan(x))r.pf=x;
 Serial.printf("%s: %.3f kWh, %.1f W, relay %s\n",DEVICE_NAMES[i],r.energy,r.power,r.tripped?"OFF":"ON");
}
bool postJSON(const String &path,const String &body){
 if(WiFi.status()!=WL_CONNECTED)return false;
 HTTPClient http;http.begin(String(SERVER)+path);http.setTimeout(3500);http.addHeader("Content-Type","application/json");
 int code=http.POST(body);http.end();return code>=200&&code<300;
}
void sendSMS(const String &message){
  Serial.println("msg sending.......");
  Serial1.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(1000);  // Delay of 1000 milli seconds or 1 second
  Serial1.println("AT+CMGS=\"Your phone number\"\r"); // Replace x with mobile number
  delay(1000);
  Serial1.println(message);// The SMS text you want to send
  delay(100);
   Serial1.println((char)26);// ASCII code of CTRL+Z
  delay(1000);
  Serial.println("msg send.......");
}
void callUser(){
  Serial.println("Calling Initializing..."); 
  delay(1000);

  Serial1.println("AT"); //Once the handshake test is successful, i t will back to OK
  
  Serial1.println("ATD+ Your phone number;"); //  change ZZ with country code and xxxxxxxxxxx with phone number to dial
  delay(20000); // wait for 20 seconds...
  Serial1.println("ATH"); //hang up
}
void notifyWarning(int i,int stage){
 Room &r=rooms[i];float threshold=stage==1?WARNING_KWH[i]:SECOND_WARNING_KWH[i];
 r.stage=stage;r.pending=true;buzzer();
 String msg="Energy warning "+String(DEVICE_NAMES[i])+": "+String(r.energy,3)+" kWh; threshold "+String(threshold,3)+" kWh. Await dashboard trip decision.";
 sendSMS(msg);
 delay(5000);
 callUser();
 JsonDocument doc;// Keep room1/room2/room3 as Flask API identifiers; use device_name for display.
 doc["room"]="room"+String(i+1);doc["room_number"]=i+1;doc["device_name"]=DEVICE_NAMES[i];doc["power"]=r.power;doc["energy"]=r.energy;
 doc["power_limit"]=0;doc["energy_limit"]=threshold;doc["reason"]=stage==1?"ENERGY_LIMIT_EXCEEDED":"SECOND_ENERGY_LIMIT_EXCEEDED";
 String body;serializeJson(doc,body);
 if(!postJSON("/api/alert",body))Serial.printf("%s alert HTTP failed\n",DEVICE_NAMES[i]);
}
void checkThreshold(int i){
 Room &r=rooms[i];if(!r.communication||!r.energyValid||r.tripped)return;
 // Stage 1 first, then stage 2; both are notifications only.
 if(r.stage==0&&r.energy>=WARNING_KWH[i])notifyWarning(i,1);
 else if(r.stage==1&&r.energy>=SECOND_WARNING_KWH[i])notifyWarning(i,2);
}
void sendData(){
 JsonDocument doc;float totalPower=0,totalEnergy=0;int active=0;
 for(int i=0;i<COUNT;i++){
  Room &r=rooms[i];JsonObject d=doc["room"+String(i+1)].to<JsonObject>();
  d["address"]=String("0x")+String(PZEM_ADDRESSES[i],HEX);d["voltage"]=r.voltage;d["current"]=r.current;d["power"]=r.power;d["energy"]=r.energy;
  d["frequency"]=r.frequency;d["pf"]=r.pf;d["relay"]=r.tripped?0:1;d["pending"]=r.pending?1:0;d["trip"]=r.tripped?1:0;d["communication"]=r.communication?1:0;d["trip_remaining_seconds"]=r.tripped?(TRIP_DURATION_MS-(millis()-r.tripStarted))/1000:0;
  totalPower+=r.power;totalEnergy+=r.energy;if(!r.tripped)active++;
 }
 doc["total_power"]=totalPower;doc["total_energy"]=totalEnergy;doc["active_rooms"]=active;doc["wifi_rssi"]=WiFi.RSSI();
 String body;serializeJson(doc,body);postJSON("/api/update",body);
}
void restoreTimedRelays(){
 unsigned long now=millis();
 for(int i=0;i<COUNT;i++){
  Room &r=rooms[i];
  if(r.tripped && (unsigned long)(now-r.tripStarted)>=TRIP_DURATION_MS){
   r.tripped=false;r.tripStarted=0;r.pending=false;
   digitalWrite(RELAY_PINS[i],RELAY_ON);
   Serial.printf("%s relay restored ON after 60 seconds\n",DEVICE_NAMES[i]);
  }
 }
}
void pollCommands(){
 if(WiFi.status()!=WL_CONNECTED)return;
 HTTPClient http;http.begin(String(SERVER)+"/api/trip-commands");http.setTimeout(3000);
 int code=http.GET();if(code!=200){http.end();return;}
 String payload=http.getString();http.end();JsonDocument doc;if(deserializeJson(doc,payload))return;
 for(int i=0;i<COUNT;i++){
  // Flask command keys remain room1/room2/room3 for compatibility.
  String key="room"+String(i+1);String cmd=doc[key].as<String>();cmd.toUpperCase();
  if(cmd!="ALLOW"&&cmd!="DENY")continue;
  Room &r=rooms[i];if(cmd=="ALLOW" && !r.tripped){r.tripped=true;r.tripStarted=millis();}
  r.pending=false;maintainRelays();
  JsonDocument ack;ack["room"]=key;ack["device_name"]=DEVICE_NAMES[i];ack["action"]=cmd;String body;serializeJson(ack,body);
  postJSON("/api/trip-ack",body);
  Serial.printf("%s user decision: %s\n",DEVICE_NAMES[i],cmd.c_str());
 }
}
void setup(){
 Serial.begin(115200);Serial2.begin(9600,SERIAL_8N1,PZEM_RX,PZEM_TX);
 Serial1.begin(9600,SERIAL_8N1,GSM_RX,GSM_TX);
 for(int i=0;i<COUNT;i++){pinMode(RELAY_PINS[i],OUTPUT);digitalWrite(RELAY_PINS[i],RELAY_ON);}
 pinMode(BUZZER_PIN,OUTPUT);digitalWrite(BUZZER_PIN,LOW);
 //Serial1.println("AT");delay(200);Serial1.println("ATE0");
 WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
 Serial.println("3-device energy warning system. NO automatic relay cutoff.");
}
void loop(){
 unsigned long now=millis();
 if(buzzerUntil&&((long)(now-buzzerUntil)>=0)){digitalWrite(BUZZER_PIN,LOW);buzzerUntil=0;}
 if(WiFi.status()!=WL_CONNECTED){static unsigned long retry=0;if(now-retry>10000){retry=now;WiFi.reconnect();}}
 restoreTimedRelays();
 if(now-lastRead>=READ_MS){lastRead=now;for(int i=0;i<COUNT;i++){readRoom(i);delay(100);}for(int i=0;i<COUNT;i++)checkThreshold(i);maintainRelays();}
 if(now-lastCommand>=COMMAND_MS){lastCommand=now;pollCommands();}
 if(now-lastPost>=POST_MS){lastPost=now;sendData();}
 delay(10);
}
