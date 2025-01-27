/**
Author: Noah Roberts
Last Update: 01/26/25
Version Number: v5
Purpose:  This code communicates with an LM75A temperature sensor over I2C.
          It also allows the user to communicate with the device in real time
          via the serial terminal.

          Baud Rate to use: 9600
          I2C clock rate to use: 100kHz
*/

#include <Wire.h>

byte temp_c; 
byte temp_f;

int outputFlag = 0;

//Custom T_os value in celcius (Note: Value can not be negative and must be a hole integer in range of [0, 127])
int user_T_OS = 30;
int user_T_hyst = 29;

//Loop delay time (ms)
int loop_delay = 1000;

void setup() {
  Serial.begin(9600);     //Initialize the Serial Output at 9600baud
  delay(1000);
  Wire.begin();           //Initialize the Arduino as a controller device
  Wire.setClock(100000);  //Set I2C clock to 100kHz (Standard Mode)

  Serial.println(F("\nInitializing..."));
  change_OS_temp(user_T_OS);  //Configure the T_os value for Overtemp alert on start-up
  setConfigConditions();

  //Set data variable for use later
  temp_c = 0;
  temp_f = 0;

  pinMode(13, INPUT_PULLUP); //Set pin 13 as an input to monitor the OS pin on the chip (temporary; used for debugging)

  Serial.println(F("Ready for commands:"));
  printHelpMenu();
}

void loop() {
  //Read ambient temperature at start of loop
  if (outputFlag == 1) {
    readTemperature();
  }

  //Check for serial input and process command if there is one
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');  //Read command
    command.trim(); //Trim whitespace
    processCommand(command);  //Process command
  }

  //Wait for loop_delay (ms) and repeat process
  delay(loop_delay);
}


/**
 *  @brief Processes serial command entered by user and calls respective function
 *  
 *  This function takes a user input in and processes it, searching the command database for a match
 *  If no match exists, function will output a message to the user and will return, continuing normal operation
 *  
 * @param command The user entered command to be processed
 * @return None
 *
 * @note In order to support more commands, add to the else-if tree (make sure to update the initial command list output as well as the HELP command output)
 */
void processCommand(String command) {
  if (command.startsWith("SET_OS")) {
      // Extract the substring after "SET_OS"
      String osString = command.substring(7);
      osString.trim(); // Remove extra spaces or line breaks

      // Check for an equals sign and remove it if present
      int equalsIndex = osString.indexOf('=');
      if (equalsIndex != -1) {
        osString = osString.substring(equalsIndex + 1); // Take everything after the '='
      }

      // Convert to integer
      int new_OS = osString.toInt();

      // Validate the overtemp threshold value
      if (new_OS < 0 || new_OS > 127) {
        Serial.println(F("Error: Overtemp threshold out of range. Please enter a value between 0 and 127."));
        return; // Exit the command processing
      }

      // Set the overtemp threshold
      change_OS_temp(new_OS);
      user_T_OS = new_OS;
    
      Serial.print("Overtemp threshold set to: ");
      Serial.println(new_OS);
  
  } else if (command.startsWith("SET_DELAY")) {
      // Extract the substring after "SET_DELAY"
      String delayString = command.substring(10);
      delayString.trim(); // Remove extra spaces or line breaks

      // Check for an equals sign and remove it if present
      int equalsIndex = delayString.indexOf('=');
      if (equalsIndex != -1) {
        delayString = delayString.substring(equalsIndex + 1); // Take everything after the '='
      }

      // Convert to integer
      int new_delay = delayString.toInt();

      // Validate the converted integer
      if (new_delay <= 0) {
        Serial.println(F("Error: Invalid delay entered. Please enter a positive integer."));
        return; // Exit the command processing
      }

      // Ensure the delay is at least 10 ms
      if (new_delay < 10) {
        Serial.println(F("Warning: New delay entered is lower than the minimum allowed. New delay has been rounded up to the minimum (10ms)."));
        new_delay = 10;
      }

      loop_delay = new_delay;

      Serial.print(F("Loop delay set to: "));
      Serial.println(new_delay);

  } else if (command.startsWith("HELP")) {
      printHelpMenu();

  } else if (command.startsWith("GET_TEMP")) {
      readTemperature();

  } else if (command.startsWith("SET_HYST")) {
      // Extract the substring after "SET_HYST"
      String hystString = command.substring(9);
      hystString.trim(); // Remove extra spaces or line breaks

      // Check for an equals sign and remove it if present
      int equalsIndex = hystString.indexOf('=');
      if (equalsIndex != -1) {
        hystString = hystString.substring(equalsIndex + 1); // Take everything after the '='
      }

      // Convert to integer
      int new_hyst = hystString.toInt();

      // Validate the hyst threshold value
      if (new_hyst < 0 || new_hyst > 127) {
        Serial.println(F("Error: Hyst value out of range. Please enter a value between 0 and 127."));
        return; // Exit the command processing
      }

      change_tHyst_temp(new_hyst);
      user_T_hyst = new_hyst;

      Serial.print(F("T_hyst set to: "));
      Serial.println(new_hyst);   

  } else if (command.startsWith("PAUSE")) {
      pauseOutput();
  } else if (command.startsWith("START")) {
      resumeOutput();
  } else {
      Serial.print(F("Error - Unknown command: "));
      Serial.println(command);
  }
}

/**
 *  @brief Communicates with the I2C device and prints out response to serial terminal
 * 
 * @param None 
 * @return None
 */
void readTemperature() {
  int bytesReceived = Wire.requestFrom(0x48, 2);
  if (bytesReceived == 2) {
    temp_c = Wire.read();
    temp_f = ((temp_c * 1.8) + 32);

    int pinState = digitalRead(13);
    String pinStateStr = (pinState == HIGH) ? "LOW" : "HIGH"; //Device configured in Active-Low mode, so polarity on the terminal output is swapped for easy reading

    Serial.print(F("Temperature: "));
    Serial.print(temp_c);
    Serial.print(F("C; "));
    Serial.print(temp_f);
    Serial.print(F("F; OS: "));
    Serial.print(user_T_OS);
    Serial.print(F("C; Hyst: "));
    Serial.print(user_T_hyst);
    Serial.print(F("C; Overtemp Signal: "));
    Serial.println(pinStateStr);

  } else {
    Serial.print(F("~~~ ByteMismatchError: Bytes Received: "));
    Serial.println(bytesReceived);
  }
}

/**
 * @brief Communicate with I2C device and adust the Overtemp signal threshold
 *  
 * @param custom_T_os The user-entered temperature to change the Overtemp signal threshold to
 * @return None
 *
 * @note New OS temp must be between 0C and 127C, if it is not the function will automatically clamp it to be within bounds
 */
void change_OS_temp(int custom_T_os) {
  //Checks if user-given temp is within bounds, if not clamps actual threshold temp within bounds
  if (custom_T_os < 0) {
    custom_T_os = 0;
  } else if (custom_T_os > 127) {
    custom_T_os = 127;
  }

  change_to_OS_reg();
  Wire.write(custom_T_os);
  Wire.endTransmission();

  change_to_temp_reg(); //Switch back to temperature register
}

/**
 * @brief Communicate with I2C device and adust the T-hyst signal threshold
 *  
 * @param custom_T_hyst The user-entered temperature to change the T-hyst signal threshold to
 * @return None
 *
 * @note New OS temp must be between 0C and 127C, if it is not the function will automatically clamp it to be within bounds
 */
void change_tHyst_temp(int custom_T_hyst) {
  if (custom_T_hyst < 0) {
    custom_T_hyst = 0;
  } else if (custom_T_hyst > 127) {
    custom_T_hyst = 127;
  }

  change_to_tHyst_reg();
  Wire.write(custom_T_hyst);
  Wire.endTransmission();

  change_to_temp_reg(); //Switch back to temperature register  
}

/**
 * @brief Communicate with I2C device and switch to the normal operating register
 *  
 * @param None
 * @return None
 */
void change_to_temp_reg() {
  Serial.println(F("Switching to temp_register..."));
  Wire.beginTransmission(0x48);
  Wire.write(0x00);
  int switchBack_flag = Wire.endTransmission();
  
  if (switchBack_flag != 0) {
    Serial.println(F("~~~ Error switching back!\n"));
  } else if (switchBack_flag == 0) {
    Serial.println(F("~~~ Switching back success!\n"));
  }  
}

/**
 * @brief Communicate with I2C device and switch to the configuration register
 *  
 * @param None
 * @return None
 */
void change_to_configuration_reg() {
  Serial.println(F("Switching to config_register..."));
  Wire.beginTransmission(0x48);
  Wire.write(0x01);
}

/**
 * @brief Communicate with I2C device and switch to the T_hyst register
 *  
 * @param None
 * @return None
 */
void change_to_tHyst_reg() {
  Serial.println(F("Switching to tHyst_register..."));
  Wire.beginTransmission(0x48);
  Wire.write(0x02);
}

/**
 * @brief Communicate with I2C device and switch to the OS register
 *  
 * @param None
 * @return None
 */
void change_to_OS_reg() {
  Serial.println(F("Switching to OS_register..."));
  Wire.beginTransmission(0x48);
  Wire.write(0x03);  
}

/**
 * @brief Print help menu to the serial terminal
 *  
 * @param None
 * @return None
 */
void printHelpMenu() {
  Serial.println(F("\n---------------------------------------------------------------"));
  Serial.println(F("Commands:"));
  Serial.println(F("  SET_OS <value> - Set Overtemp Signal threshold (0-127)"));
  Serial.println(F("  SET_DELAY <ms> - Set loop delay in milliseconds"));
  Serial.println(F("  SET_HYST <value> - Set T_hyst value (0 - 127)"));
  Serial.println(F("  GET_TEMP - Output current temperature to terminal"));
  Serial.println(F("  PAUSE - Pauses temp polling until START command is typed in"));
  Serial.println(F("  START - Starts temp polling until PAUSE command is typed in"));
  Serial.println(F("  HELP - Output list of valid commands"));
  Serial.println(F("---------------------------------------------------------------\n"));  
}

void setConfigConditions() {
  change_to_configuration_reg();
  Wire.write(0x08);
  Wire.endTransmission();

  change_to_temp_reg();

  Serial.println(F("Device Configuration complete: 0x08"));
  Serial.println(F("Configuration: 2 OS faults, OS Active-Low, Comparator Mode, not in low-power mode"));
}

void pauseOutput() {
  outputFlag = 0;
}

void resumeOutput() {
  outputFlag = 1;
}
