**DIY Smart Lock System with RFID and Web Interface**

a smart lock system designed to lock and unlock using RFID readers, a web interface, and a stepper motor. The system is built using an ESP32 microcontroller, TMC2208 stepper driver, and RFID modules (PN532 or RC522). It also includes a built-in delay mechanism for unauthorized attempts and a buzzer for feedback.

## Components Used

- **Microcontroller**: ESP32-C3 supermini. NOTE: only currently works with esp32 by Espressif systems versio 3.0.7 
- **Stepper Driver**: TMC2208
- **RFID Readers**: PN532 or RC522
- **Stepper Motor**
- **Buzzer**
- **Limit Switches**: Detect locked and unlocked states
- **Jumper Pins**: Enables AP mode for web access

![image](https://github.com/user-attachments/assets/a6cc4600-7b9b-492e-b48b-83efec29ab52)


### Software Setup

1. **Dependencies**:
   - Install the following libraries via Arduino IDE Library Manager:
     - `TMCStepper`
     - `Adafruit_PN532`
     - `MFRC522`
     - `ESPAsyncWebServer`
     - `ArduinoJson`
     - `LittleFS`

2. **Code Configuration**:
   - Update the **WiFi credentials** in the `setup()` function if using SoftAP mode.
   - Adjust stepper motor current (`STEPPER_CURRENT`) and step time (`STEP_TIME`) as needed for your motor.

3. **Upload Code**:
   - Select the appropriate ESP32 board and upload the code via the Arduino IDE.

4. **File System Setup**:
   - Format the ESP32's SPIFFS using the **LittleFS** library.
   - Ensure `uids.json` exists in the root directory or is created automatically.

---

## Usage

### Lock/Unlock Mechanism

1. Present a valid RFID card to the reader.
   - The system will automatically lock or unlock depending on the current state.
   - An invalid card triggers a buzzer alert and delays further access.

2. Bridge the jumper pins to enable web configuration mode.

### Web Interface

1. Connect to the ESP32's WiFi network (`ChimpoLock`, password: `Password123`).
2. Open a browser and navigate to `http://192.168.4.1/`.
3. **Features**:
   - View stored UIDs.
   - Add a new UID with a name.
   - Remove an existing UID by clicking the remove button.

---

## Bruteforce Security

- Unauthorized attempts are delayed using the Fibonacci sequence for increasing delay durations.
- Cards not registered in `uids.json` are denied access.

---

## License

This project is released under the [MIT License](LICENSE). 

Contributions and suggestions are welcome! 🎉
