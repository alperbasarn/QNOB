#include <Preferences.h>

void updateSerial() {
  // Check if data is available on the serial port
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');  // Read until newline
    input.trim();                                 // Remove any extra spaces or newlines
    Serial1.print(input);  // Use Serial1 instead of softSerial

    // Check for "P=" command
    if (input.startsWith("kp=")) {
      float newKp = input.substring(3).toFloat();
      Kp = newKp;
      pidController.SetTunings(Kp, Ki, Kd);
      Serial.print("Updated Kp: ");
      Serial.println(newKp);
    }
    // Check for "VI=" command
    else if (input.startsWith("ki=")) {
      float newKi = input.substring(3).toFloat();
      Ki = newKi;
      pidController.SetTunings(Kp, Ki, Kd);
      Serial.print("Updated Ki: ");
      Serial.println(Ki);
    } else if (input.startsWith("pidsampletime=")) {
      float newSampleTime = input.substring(14).toFloat();
      pidSampleTime = newSampleTime;
      pidController.SetSampleTime(pidSampleTime);
      Serial.print("Updated pid sample time: ");
      Serial.println(newSampleTime);
    } else if (input.startsWith("voltagelimit=")) {
      float newMaxVoltage = input.substring(13).toFloat();
      motorVoltageLimit = newMaxVoltage;
      motor.voltage_limit = motorVoltageLimit;  // Voltage limit
      motor.init();
      Serial.print("Updated max voltage: ");
      Serial.println(newMaxVoltage);
    } else if (input.startsWith("velocitylimit=")) {
      float newMaxVelocity = input.substring(14).toFloat();
      motorVelocityLimit = newMaxVelocity;
      motor.velocity_limit = motorVelocityLimit;  // Voltage limit
      motor.init();
      Serial.print("Updated max velocity: ");
      Serial.println(newMaxVelocity);
    } else if (input.startsWith("poles=")) {
      int newPoles = input.substring(6).toInt();  // Corrected parsing
      NUM_POLES = newPoles;
      Serial.print("Updated poles: ");
      Serial.println(newPoles);
      POLE_ANGLE_STEP = 360 / NUM_POLES;
      //HYSTERESIS_THRESHOLD = calculateDeadband() / 2;
    } else if (input.startsWith("deadband=")) {
      float newDeadBand = input.substring(9).toFloat();
      deadbandCoefficient = newDeadBand;
      Serial.print("Updated deadband: ");
      Serial.println(newDeadBand);
      //HYSTERESIS_THRESHOLD = calculateDeadband() / 2;
    } else if (input.startsWith("brightness=")) {
      int newBrightness = input.substring(11).toInt();
      brightness = newBrightness;
      Serial.print("Updated led brightness: ");
      Serial.println(brightness);
    } else if (input.startsWith("setpoint=")) {
      int newSetpoint = input.substring(9).toInt();
      setPoint = newSetpoint;
      Serial.print("Updated setpoint: ");
      Serial.println(setPoint);
    } else if (input.startsWith("color=")) {
      int newColor = input.substring(6).toInt();
      currentColor = newColor;
      Serial.print("Updated led color: ");
      Serial.println(currentColor);
    } else if (input == "disable") {
      motor.disable();
      Serial.print("disabled motor.");
    } else if (input == "enable") {
      motor.enable();
      Serial.print("enabled motor.");
    } else if (input == "+") {
      setIncreaseAnimate();
    } else if (input == "-") {
      setDecreaseAnimate();
    } else if (input == "calibrate") {
      Serial1.print(input);  // Use Serial1 instead of softSerial
      Serial.println("sent calibration command.");
    } else {
      Serial.println("Invalid command. Use P=, I=, or D= to set parameters.");
    }
  }
}

void initializeSoftSerial() {
  // Initialize hardware serial (Serial1) instead of SoftwareSerial
  Serial1.begin(9600, SERIAL_8N1, SERIAL_RX_PIN, SERIAL_TX_PIN);
  Serial.println("Hardware Serial initialized.");
}

void updateSoftSerial() {
  // Check if data is available from the secondary serial
  while (Serial1.available()) {  // Use Serial1 instead of softSerial
    String receivedMessage = "";
    while (Serial1.available()) {  // Use Serial1 instead of softSerial
      char c = Serial1.read();  // Use Serial1 instead of softSerial
      if (c == '\n') break;  // End of message
      receivedMessage += c;
      delay(3);  // Prevent buffer overflow
    }

    //Serial.print("📩 Received Message: ");
    //Serial.println(receivedMessage);

    // If the received message is "zeroPos", save current_angle
    if (receivedMessage == "zeroPos") {
      Serial.println("🔹 Saving current angle as zero position to EEPROM:");
      Serial.println(getRawAngle());
      //sensor.burnZeroAngle();
      Serial.print("✅ Zero position saved: ");
      Serial.println(getRawAngle());
      setIncreaseAnimate();
    } else if (receivedMessage.startsWith("setpoint=")) {
      int newSetpoint = receivedMessage.substring(9).toInt();
      setPoint = newSetpoint;
      Serial.print("Updated setpoint: ");
      Serial.println(setPoint);
    } else if (receivedMessage == "sp?") {
      Serial1.print("setpoint="+String(setPoint));  // Use Serial1 instead of softSerial
      Serial.println("setpoint query received. sent setpoint:" + String(setPoint));
    }
  }
}