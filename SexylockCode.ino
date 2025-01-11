//NOTE only currently works with esp32 by Espressif systems versio 3.0.7 do not update!!

#include <TMCStepper.h>

#define STEPPER_CURRENT      900 // mA, do not exceed 900
#define STEP_TIME            500 // microseconds per half step (lower is faster)
#define HIGH_STEPPER_CURRENT 1000 // mA when trying to overcome jam, do not exceed 1000
#define SLOW_STEP_TIME       800 // microseconds per half step when trying to overcome jam

#define BZ_PIN        10 // buzzer 
#define EN_PIN           2 // Enable pin 
#define DIR_PIN          3 // Direction 
#define STEP_PIN         9 // Step 

#define MICROSTEPS        8 // Options are 0,2,4,8,16,32,64,128,256 
#define R_SENSE       0.11f
#define SERIAL_PORT Serial1 // Hardware Serial Port
TMC2208Stepper driver(&SERIAL_PORT, R_SENSE);  //  // Hardware Serial (TX / RX pins)

#define MAX_STEPS_NORMAL 1920 
#define MAX_STEPS_SLOW   1630

#define AP_BUTTON 8
#define LOCKED_SWITCH 0
#define UNLOCKED_SWITCH 1
bool checkLocked; // Limit switch value for locked position
bool checkUnlocked; // Limit switch value for unlocked position

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Constants.h>

#define PN532_SCK (4)
#define PN532_MOSI (6)
#define PN532_SS (7)
#define PN532_MISO (5)
#define RC522_SS 7     // Chip select for RC522

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
MFRC522DriverPinSimple ss_pin(RC522_SS); // Create pin driver. See typical pin layout above.
SPIClass &spiClass = SPI; // Alternative SPI e.g. SPI2 or from library e.g. softwarespi.
const SPISettings spiSettings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0); // May have to be set if hardware is not fully compatible to Arduino specifications.
MFRC522DriverSPI rc522driver{ss_pin, spiClass, spiSettings}; // Create SPI driver.
MFRC522 mfrc522{rc522driver};

bool PN532_READER;
bool RC522_READER;

#define MAXIMUM_INVALID_ATTEMPTS 13
uint8_t invalidAttempts = 1;
const uint8_t invalidDelays[MAXIMUM_INVALID_ATTEMPTS] = { 1, 2, 3, 5, 8, 13, 21, 33, 54};

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
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
    const AsyncWebParameter* p = request->getParam("uid");
    uint8_t index = p->value().toInt();

    File file = LittleFS.open("/uids.json", "r");
    ReadBufferingStream bufferedFileR(file, 64);

   DynamicJsonDocument doc(ESP.getMaxAllocHeap());
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
    const AsyncWebParameter* puid = request->getParam("uid");
    const AsyncWebParameter* pname = request->getParam("name");

    Serial.print("ADD UID - ");
    Serial.print("Name: ");
    Serial.println(pname->value());
    Serial.print("UID: ");
    Serial.println(puid->value());

    File file = LittleFS.open("/uids.json", "r");
    ReadBufferingStream bufferedFileR(file, 64);

    DynamicJsonDocument doc(ESP.getMaxAllocHeap());
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
  checkLocked = digitalRead(LOCKED_SWITCH);
  checkUnlocked = digitalRead(UNLOCKED_SWITCH);
}

void beep(int number, long time) {
  for (int i = 0; i < number; i++) {
    tone(BZ_PIN, 440);
    delay(time);
    noTone(BZ_PIN);
    delay(time);
  }
}

void moveLock(bool dir) {
  driver.toff(5);
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
  //if button here for changing motor direction and toggle switch setup

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
}

bool detectRC522() {
  mfrc522.PCD_Init();
  delay(100);
  MFRC522Constants::PCD_Version version = mfrc522.PCD_GetVersion();
  if(version == MFRC522Constants::Version_Unknown) {
    return false;
  } else {
    return true;
  }
}

bool detectPN532() {
  nfc.begin();
  delay(100);
  uint32_t version = nfc.getFirmwareVersion();
  
  if (version) {
    Serial.print(F("Found chip PN5")); Serial.println((version>>24) & 0xFF, HEX); 
    Serial.print(F("Firmware ver. ")); Serial.print((version>>16) & 0xFF, DEC); 
    Serial.print('.'); Serial.println((version>>8) & 0xFF, DEC);
  }
  return (version);  // Return true if detected
}

void setup() {
  PN532_READER = false;
  RC522_READER = false;
  Serial.begin(115200);
  delay(500);
  Serial.print(F("Start of program"));
  
  SERIAL_PORT.begin(115200, SERIAL_8N1, 21, 20);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  
  driver.begin();
  driver.pdn_disable(true);
  driver.rms_current(STEPPER_CURRENT);
  driver.microsteps(MICROSTEPS);
  driver.en_spreadCycle(true);
  driver.pwm_autoscale(true);
  driver.I_scale_analog(false);
  driver.irun(16);
  driver.toff(0);

  pinMode(AP_BUTTON, INPUT_PULLUP);
  pinMode(LOCKED_SWITCH, INPUT_PULLUP);
  pinMode(UNLOCKED_SWITCH, INPUT_PULLUP);

  readSwitches();

  SPI.begin(); // Initiate  SPI bus
  if (detectRC522()) {
    RC522_READER = true;
    beep(2, 200);  // Two beeps for PN532
    Serial.println("rc522 detected.");
    
  } else if  (detectPN532()) { 
    PN532_READER = true;
    beep(3, 100);  // Three beeps for RC522
    Serial.println("PN532 detected.");
  } else {
    beep(1, 500);  // Long beep if no reader is found
    Serial.println("No RFID reader detected.");
    while (true);  // Halt execution if no reader is found
  }
  
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    LittleFS.format();
    LittleFS.begin();
  }
  checkJSON();
  
  
  if (!digitalRead(AP_BUTTON)) {
    Serial.println("Softap Active, Please log into lock via smartphone.");
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
    } else { 
    beep(1, 200);
    Serial.println("Softap not Active,Lock mode Enabled");
  }
	
  Serial.println(F("Waiting for an ISO14443A card"));
}

void loop() {

  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 , 0};  // Buffer to store the returned UID
  uint8_t uidLength;
  bool validUID = false;


  if (PN532_READER) {
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  } else if (RC522_READER) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      success = true;
      uidLength = mfrc522.uid.size;
      for (int i = 0; i < uidLength; i++) {
        uid[i] = mfrc522.uid.uidByte[i];
      }
      mfrc522.PICC_HaltA();
    }
  }

  if (!success) {
    delay(500);
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

		DynamicJsonDocument doc(ESP.getMaxAllocHeap());
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
    delay(1000); // added delay to stop double scan
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
