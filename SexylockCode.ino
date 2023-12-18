#include <TMCStepper.h>

//https://github.com/clarketronics/PN532/archive/refs/heads/master.zip pn532 library
//==========================================================================
//                   Things for Chimpo to play with
//==========================================================================

#define STEPPER_CURRENT      700 // mA, do not exceed 900
#define STEP_TIME            400 // microseconds per half step (lower is faster)
#define HIGH_STEPPER_CURRENT 900 // mA when trying to overcome jam, do not exceed 1000
#define SLOW_STEP_TIME       800 // microseconds per half step when trying to overcome jam

#define NUMBER_4BYTE_UIDS 6
#define NUMBER_7BYTE_UIDS 3

uint8_t uids4B[NUMBER_4BYTE_UIDS][4]  = {  
  {0x00, 0x00, 0x00, 0x00}, // MAM
  {0x00, 0x00, 0x00, 0x00}, // ME
  {0x00, 0x00, 0x00, 0x00}, // CALLUM
  {0x00, 0x00, 0x00, 0x00}, // HAYLEY
  {0x00, 0x00, 0x00, 0x00}, // ELSIE
  {0x00, 0x00, 0x00, 0x00}, // MIKE
};

uint8_t uids7B[NUMBER_4BYTE_UIDS][7] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // MY HAND
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // JEN RING
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // MY HAND V2
};

//==========================================================================
//                     Pin and object configuration
//==========================================================================

#define EN_PIN           16 // Enable pin (D0) - GPIO16)
#define DIR_PIN          4 // Direction (D2 - GPIO 4)
#define STEP_PIN         5 // Step (D1 - GPIO 5)
#define TOFF              5
#define MICROSTEPS        8 // Options are 0,2,4,8,16,32,64,128,256 
#define SPREADCYCLE    true // Options are true or false
#define R_SENSE       0.11f
#define DRIVER_ADDRESS 0b00 // TMC2209 Driver address according to MS1 and MS2
#define SERIAL_PORT Serial // Hardware Serial Port
TMC2209Stepper driver(&SERIAL_PORT, R_SENSE, DRIVER_ADDRESS); // Hardware Serial (TX / RX pins)

#define MAX_STEPS_NORMAL 1920 
#define MAX_STEPS_SLOW   1630

#define AP_VAL 559
#define LOCKED_VAL  376
#define UNLOCKED_VAL 286
#define LOCKED_SWITCH 9 //(sd2)
#define UNLOCKED_SWITCH 0// (sd3)
bool checkLocked; // Limit switch value for locked position
bool checkUnlocked; // Limit switch value for unlocked position

#include <SPI.h>
#include <PN532_SPI.h>
#include "PN532.h"

PN532_SPI pn532spi(SPI, 2);//(D4 SS - GPIO2)
PN532 nfc(pn532spi);

#define MAXIMUM_INVALID_ATTEMPTS 13
uint8_t invalidAttempts = 0;
const uint8_t invalidDelays[MAXIMUM_INVALID_ATTEMPTS] = {1, 1, 1, 1, 1, 2, 3, 5, 8, 13, 21, 33, 54};

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <StreamUtils.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

AsyncWebServer server(80);

const char index_html[] PROGMEM = "<!DOCTYPE html><html><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>h1{text-align:center;margin:auto;color:#2bca23;text-shadow:0 0 5px #26ac1f,0 0 20px #26ac1f,0 0 30px #26ac1f}.line{cursor:default;position:relative;font-size:50px}.line:after{content:'';position:absolute;width:100%;height:5px;bottom:-10px;left:-4px;transform:scaleX(0);box-shadow:0 -5px 0 #26ac1f6d;background:#20711969;transform-origin:bottom right;transition:transform .5s ease-out}.line:hover:after{transform:scaleX(1);transform-origin:bottom left}body{background-image:linear-gradient(to right,#2c2b2b,#1a1b1b,#403f3f,#403f3f,#393939,#252525)}h2{color:#26ac1f;text-shadow:0 0 20px #26ac1f;text-align:center}#uidtable{text-align:center;margin:auto}th{color:#26ac1f;text-shadow:0 0 5px #26ac1f}td{color:#26ac1f;text-shadow:0 0 5px #26ac1f}.button{color:red;text-shadow:0 0 10px red}.button:hover{text-shadow:0 0 10px red,0 0 20px red,0 0 35px red,0 0 30px red}#uidaddname,#uidadduid{padding-bottom:12px}label{align-items:center;margin:auto;display:block;margin-bottom:2px}input[type=text]{align-items:center;margin:auto;display:block;background-color:#343434;color:#45ca3e;border-color:#000}input[type=text]:focus{outline:0}input[type=submit]{color:#40cb65;background-color:#3b3a3a}input[type=submit]:hover{color:#000;background-color:#30b127}form{color:#34a351;text-shadow:0 0 5px #26ac1f,0 0 20px #26ac1f;text-align:center;margin:auto}</style><script>document.addEventListener(\"readystatechange\",(function(e){\"interactive\"==e.target.readyState&&fetch(\"uids.json\").then((e=>{if(!e.ok)throw new Error(\"HTTP error \"+e.status);return e.json()})).then((e=>{let t=document.getElementById(\"uidtable\");for(let n=0,r=e.length;n<r;n++){let r=e[n],o=t.insertRow(-1);o.insertCell(-1).appendChild(document.createTextNode(r.name)),o.insertCell(-1).appendChild(document.createTextNode(r.uid)),o.insertCell(-1).innerHTML='<a href=\"remove?uid='+n+'\" class=\"button\">&#x2715;</a>'}document.getElementById(\"loading\").style.display=\"none\"})).catch((function(){console.log(\"Error fetching UIDs JSON\")}))}))</script><body><main><h1 class=\"line\">chimpolock</h1><div id=\"uidlist\"><h2>Stored UIDs:</h2><table id=\"uidtable\"><tr><th>Name</th><th>UID</th><th>Remove</th></tr></table><h3 id=\"loading\">Loading UIDs, please wait...</h3></div><div id=\"uidadd\"><h2>Add UID:</h2><form action=\"/add\" method=\"get\"><div id=\"uidaddname\"><label for=\"name\">Name:</label> <input type=\"text\" name=\"name\" id=\"name\" required></div><div id=\"uidadduid\"><label for=\"uid\">UID:</label> <input type=\"text\" name=\"uid\" id=\"uid\" required></div><input type=\"submit\" value=\"Add\"></form></div></main></body></html>";
void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_html);
}

void checkJSON() {
  File file = LittleFS.open("/uids.json", "r");
  if (!file) {
    file.close();
    Serial.println("Creating new uids.json");
    File filew = LittleFS.open("/uids.json", "w");
    filew.print("[]");
    filew.close();
  } else {
    file.close();
  }
}

void handleJSON(AsyncWebServerRequest *request) {
  request->send(LittleFS, "/uids.json");
}

void handleRemove(AsyncWebServerRequest *request) {
  if(request->hasParam("uid")){
    AsyncWebParameter* p = request->getParam("uid");
    uint8_t index = p->value().toInt();

    File file = LittleFS.open("/uids.json", "r");
    ReadBufferingStream bufferedFileR(file, 64);

    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 1024);
    deserializeJson(doc, bufferedFileR);
    doc.shrinkToFit();
    
    file.close();

    doc.as<JsonArray>().remove(index);

    File filew = LittleFS.open("/uids.json", "w");
    WriteBufferingStream bufferedFileW(filew, 64);
    serializeJson(doc, bufferedFileW);
    bufferedFileW.flush();
    filew.close();
  }

  request->redirect("/");
}

void handleAdd(AsyncWebServerRequest *request) {  
  if(request->hasParam("uid") && request->hasParam("name")){
    AsyncWebParameter* puid = request->getParam("uid");
    AsyncWebParameter* pname = request->getParam("name");

    Serial.print("ADD UID - ");
    Serial.print("Name: ");
    Serial.println(pname->value());
    Serial.print("UID: ");
    Serial.println(puid->value());

    File file = LittleFS.open("/uids.json", "r");
    ReadBufferingStream bufferedFileR(file, 64);

    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 1024);
    deserializeJson(doc, bufferedFileR);
    file.close();

    JsonObject nested = doc.as<JsonArray>().createNestedObject();
    nested["name"] = pname->value();
    nested["uid"] = puid->value();

    File filew = LittleFS.open("/uids.json", "w");
    WriteBufferingStream bufferedFileW(filew, 64);
    serializeJson(doc, bufferedFileW);
    bufferedFileW.flush();
    filew.close();

    Serial.println("New JSON -");
    serializeJson(doc, Serial);
  }
  request->redirect("/");
}

void readSwitches() {
  checkLocked = true;
  checkUnlocked = true;
  int ADC_value = 0;
  for (int i=0; i < 10; i++){
     ADC_value += analogRead(A0);
  }
  ADC_value=  ADC_value/10;

  if ((ADC_value>LOCKED_VAL-20)&&(ADC_value<LOCKED_VAL+20)) {
    checkLocked = false;
  } else if ((ADC_value>UNLOCKED_VAL-20)&&(ADC_value<UNLOCKED_VAL+20)) {
    checkUnlocked = false;
  }
	
}

void beep(int number, long time) {
  for (int i = 0; i < number; i++) {
    tone(15, 440);//buzzer pin gpio
    delay(time);
    noTone(15);
    delay(time);
  }
}

void moveLock(bool dir) {
  driver.toff(TOFF);
  digitalWrite(DIR_PIN, dir);

  readSwitches();

  digitalWrite(EN_PIN, LOW);

  long stepCount = 0;
  while (((!dir && checkLocked) || (dir && checkUnlocked)) && stepCount < MAX_STEPS_NORMAL) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(STEP_TIME);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_TIME);

    readSwitches();
    stepCount++;
    if (stepCount % 100 == 0) {
      yield();
    }
  }
  if ((checkLocked && checkUnlocked) || (dir && checkLocked) || (!dir && checkUnlocked)) {
    stepCount = 0;
    driver.rms_current(HIGH_STEPPER_CURRENT);
    while (((!dir && checkLocked) || (dir && checkUnlocked)) && stepCount < MAX_STEPS_SLOW) {
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(SLOW_STEP_TIME);
      digitalWrite(STEP_PIN, LOW);
      delayMicroseconds(SLOW_STEP_TIME);

      readSwitches();
      stepCount++;
      if (stepCount % 100 == 0) {
        yield();
      }
    }
    driver.rms_current(STEPPER_CURRENT);
  }
  driver.toff(0);
  digitalWrite(EN_PIN, HIGH);
}

void changeState() {
  
  readSwitches();
  if (!checkUnlocked) { // checks if door is open or locked.
    beep(1, 200);
    // Clockwise
    moveLock(false);
    Serial.println("Door unlocked");
  } else {  
    beep(2, 200);
    // Anti-clockwise      
    moveLock(true);
    Serial.println("Door locked");
  }

//  if (!checkLocked) { // checks if door is open or locked.
//    beep(1, 200);
//    // Clockwise
//    moveLock(true);
//    Serial.println("Door unlocked");
//  } else {  
//    beep(2, 200);
//    // Anti-clockwise      
//    moveLock(false);
//    Serial.println("Door locked");
//  }
}

void setup() {
  Serial.begin(115200);
  SERIAL_PORT.begin(115200);
  delay(500);
  Serial.print(F("Start of program"));
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  driver.begin();
  driver.rms_current(STEPPER_CURRENT);
  driver.microsteps(MICROSTEPS);
  driver.en_spreadCycle(SPREADCYCLE);
  driver.pwm_autoscale(true);
  driver.toff(0);

//  pinMode(LOCKED_SWITCH, INPUT_PULLUP);
//  pinMode(UNLOCKED_SWITCH, INPUT_PULLUP);

  readSwitches();

  //SPI.begin(); // Initiate  SPI bus

  nfc.begin();

  delay(1000);
  Serial.println(F("Starting up!"));
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print(F("Didn't find PN53x board"));
    while (1); // halt
  }
  
  Serial.print(F("Found chip PN5")); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print(F("Firmware ver. ")); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  //nfc.setPassiveActivationRetries(0x0F);
  
  // configure board to read RFID tags
	

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    LittleFS.format();
    LittleFS.begin();
  }
  checkJSON();
	
	int ADC_value = 0;
  for (int i=0; i < 10; i++){
     ADC_value += analogRead(A0);
  }
  ADC_value =  ADC_value/10;
	
	if ((ADC_value>AP_VAL-20)&&(ADC_value<AP_VAL+20)) {
    WiFi.softAP("ChimpoLock", "Password123");
		server.on("/", HTTP_GET, handleRoot);
		server.on("/add", HTTP_GET, handleAdd);
		server.on("/remove", HTTP_GET, handleRemove);
		server.on("/uids.json", HTTP_GET, handleJSON);
		
		server.onNotFound([](AsyncWebServerRequest *request){
			request->send(404);
		});
		
    server.begin();
    beep(1, 1000);
  }
	
  nfc.SAMConfig();
  Serial.println(F("Waiting for an ISO14443A card"));
}

void loop() {

  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 , 0};  // Buffer to store the returned UID
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  bool validUID = false;

  // Awaiting an UID to be Presented
  if (!success) {
    return;
	} else {
		char uidStr[80];
    char uidStrSplit[80];
		
		if (uidLength == 4) {
			Serial.println("4B UID");
			sprintf(uidStr, "%02x%02x%02x%02x", uid[0], uid[1], uid[2], uid[3]);
      sprintf(uidStrSplit, "%02x:%02x:%02x:%02x", uid[0], uid[1], uid[2], uid[3]);
		} else if (uidLength == 7) {
			Serial.println("7B UID");
			sprintf(uidStr, "%02x%02x%02x%02x%02x%02x%02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
      sprintf(uidStrSplit, "%02x:%02x:%02x:%02x:%02x:%02x:%02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
		}
		
		Serial.print("Detected UID is: ");
		Serial.println(uidStr);
		
		File file = LittleFS.open("/uids.json", "r");
		ReadBufferingStream bufferedFileR(file, 64);

		DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 1024);
		deserializeJson(doc, bufferedFileR);
		doc.shrinkToFit();
		
		Serial.println("JSON UID listing - ");
		
		for (JsonObject item : doc.as<JsonArray>()) {
			const char* jsonName = item["name"];
			const char* jsonUid = item["uid"];
			
			Serial.println(jsonUid);
			
			if (strcasecmp(uidStr, jsonUid) == 0 || strcasecmp(uidStrSplit, jsonUid) == 0) {
				validUID = true;
			}
		}
	}

  if (validUID) {
    Serial.println("Valid UID");
    changeState();
    invalidAttempts = 0;
  } else {
    Serial.println(" Access denied");
   beep(3, 150);
    delay(invalidDelays[invalidAttempts] * 1000);
    if (invalidAttempts < MAXIMUM_INVALID_ATTEMPTS-1) {
      invalidAttempts++;
    }
  }
}
