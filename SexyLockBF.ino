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

#define LOCKED_VAL  560
#define UNLOCKED_VAL 831
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

void readSwitches() {
  checkLocked = true;
  checkUnlocked = true;
  int ADC_value = 0;
  for (int i=0; i < 10; i++){
     ADC_value += analogRead(A0);
  }
  ADC_value=  ADC_value/10;

  if ((ADC_value>LOCKED_VAL-50)&&(ADC_value<LOCKED_VAL+50)) {
    checkLocked = false;
  } else if ((ADC_value>UNLOCKED_VAL-50)&&(ADC_value<UNLOCKED_VAL+50)) {
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
  nfc.SAMConfig();
  Serial.println(F("Waiting for an ISO14443A card"));
}

void loop() {

  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;

 success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

  // Awaiting an UID to be Presented
  if (!success) {
    return;
  }
  
  bool validUID = false;
  if (uidLength == 4) {
    Serial.println("4B UID");
    for (int i = 0; i < NUMBER_4BYTE_UIDS; i++) {
      bool same = true;
      for (int j = 0; j < 4; j++) {
        if (uids4B[i][j] != uid[j]) {
          same = false;
        }
      }
      if (same) {
        validUID = true;
      }
    }
  } else if (uidLength == 7) {
    Serial.println("7B UID");
    for (int i = 0; i < NUMBER_7BYTE_UIDS; i++) {
      bool same = true;
      for (int j = 0; j < 7; j++) {
        if (uids7B[i][j] != uid[j]) {
          same = false;
        }
      }
      if (same) {
        validUID = true;
      }
    }
  } else {
    Serial.print("Unknown UID type / length");
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