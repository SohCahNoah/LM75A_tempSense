/**
Author: Noah Roberts
Last Update: 01/25/25
Version Number: v4
Purpose:  This code communicates with an LM75A temperature sensor over I2C.
          It also allows the user to communicate with the device in real time
          via the serial terminal.

          Baud Rate to use: 9600
          I2C clock rate to use: 100kHz
*/


#include <Wire.h>

byte temp_c; 
byte temp_f;
bool sensorValue;
String senseBool;

//****** Change these values depending on actual hardware implementation: ******
int tempSensor_addr = 0x48; //Represents an I2C address in hex, where the 3 LSB's are user adjustable and the 5 MSB's are hard wired (01001) with the 1st MSB being assumed 0, as it is unused
int t_os_pointer = 0x03;  //Register pointer to the register that controls the "Overtemp Signal" threshold
int temp_pointer = 0x00;  //Register pointer to the register that controls the main operation, allowing for temp calls to be made

//Custom T_os value in celcius (Note: Value can not be negative and must be a hole integer in range of [0, 127])
int user_T_OS = 30;

//Loop delay time (ms)
int loop_delay = 1000;

void setup() {
  Serial.begin(9600);     //Initialize the Serial Output at 9600baud
  Wire.begin();           //Initialize the Arduino as a controller device
  Wire.setClock(100000);  //Set I2C clock to 100kHz (Standard Mode)

  Serial.println("\nInitializing...");
  change_OS_temp(user_T_OS);  //Configure the T_os value for Overtemp alert on start-up

  //Set data variable for use later
  temp_c = 0;
  temp_f = 0;

  Serial.println("Ready for commands:");
  Serial.println("Commands:");
  Serial.println("  SET_OS <value> - Set Overtemp Signal threshold (0-127)");
  Serial.println("  SET_DELAY <ms> - Set loop delay in milliseconds");
  Serial.println("  HELP - Output list of valid commands\n");
}

void loop() {
  //Read ambient temperature at start of loop
  readTemperature();

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
      Serial.println("Error: Overtemp threshold out of range. Please enter a value between 0 and 127.");
      return; // Exit the command processing
    }

    // Set the overtemp threshold
    change_OS_temp(new_OS);

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
      Serial.println("Error: Invalid delay entered. Please enter a positive integer.");
      return; // Exit the command processing
    }

    // Ensure the delay is at least 10 ms
    if (new_delay < 10) {
      Serial.println("Warning: New delay entered is lower than the minimum allowed. New delay has been rounded up to the minimum (10ms).");
      new_delay = 10;
    }

    loop_delay = new_delay;

    Serial.print("Loop delay set to: ");
    Serial.println(new_delay);

  } else if (command.startsWith("HELP")) {
      Serial.println("\n---------------------------------------------------------------");
      Serial.println("Commands:");
      Serial.println("  SET_OS <value> - Set Overtemp Signal threshold (0-127)");
      Serial.println("  SET_DELAY <ms> - Set loop delay in milliseconds");
      Serial.println("  HELP - Output list of valid commands");
      Serial.println("---------------------------------------------------------------\n");

  } else {
    Serial.print("Error - Unknown command: ");
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
  int bytesReceived = Wire.requestFrom(tempSensor_addr, 2);
  if (bytesReceived == 2) {
    temp_c = Wire.read();
    temp_f = ((temp_c * 1.8) + 32);

    Serial.print("Temperature: ");
    Serial.print(temp_c);
    Serial.print("C; ");
    Serial.print(temp_f);
    Serial.println("F");
  } else {
    Serial.print("~~~ ByteMismatchError: Bytes Received: ");
    Serial.println(bytesReceived);
  }
}

/**
 *  @brief Communicate with I2C device and adust the Overtemp signal threshold
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

  Serial.println("\nSwitching to OS_temp_register...");
  //Initiate communication with device
  Wire.beginTransmission(tempSensor_addr);
  //Switch register to the OS pointer register
  Wire.write(t_os_pointer);
  //Adjust temperature to custom_T_os value
  Wire.write(custom_T_os);
  //Switch back to normal operating register
  //Wire.write(temp_pointer);
  //End transmission;
  Wire.endTransmission();

  change_to_temp_reg(); //Switch back to temperature register
}

/**
 *  @brief Communicate with I2C device and switch to the normal operating register
 *  
 * @param None
 * @return None
 */
void change_to_temp_reg() {
  Serial.println("Switching to temp_register...");
  Wire.beginTransmission(tempSensor_addr);
  Wire.write(temp_pointer);
  int switchBack_flag = Wire.endTransmission();
  
  if (switchBack_flag != 0) {
    Serial.println("~~~ Error switching back!\n");
  } else if (switchBack_flag == 0) {
    Serial.println("~~~ Switching back success!\n");
  }  
}