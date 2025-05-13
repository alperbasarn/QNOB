#include <Preferences.h>
#define DEG_TO_RAD (PI / 180.0)
#define RAD_TO_DEG (180.0 / PI)
double openLoopPositionTarget;
Preferences preferences;
// Magnetic sensor instance
MagneticSensorI2C sensor = MagneticSensorI2C(MT6701_I2C);
// Motor driver instance
BLDCDriver3PWM driver = BLDCDriver3PWM(6, 7, 8, 5);
// Haptic attraction controller
const float alpha = 0.6;  // Low-pass filter coefficient
int currentColorIndex = 0;
// Virtual pole configuration
float POLE_ANGLE_STEP = 360.0 / NUM_POLES;  // Angle step between poles in degrees
double closest_pole_angle;
int pole_index;
// Offset to synchronize current angle with target angle during startup
double angle_offset = 0;  // Changed to double
double angle_error;
// Previous pole index for direction detection
int previous_pole_index = 0;
int oscillation_count = 0;            // Count of oscillations between two poles
int last_pole_index = -1;             // Track the last pole index
bool motor_enabled = true;            // Flag to enable/disable the motor
const int OSCILLATION_THRESHOLD = 2;  // Number of oscillations before disabling the motor
int oscillation_pole_a = -1;          // Track the first pole in the oscillating pair
int oscillation_pole_b = -1;          // Track the second pole in the oscillating pair
double raw_angle;
static double filtered_angle = 0;        // Initialize filtered angle
static double last_angle = 0;            // Keep track of the last angle
static bool crossed_hysteresis = false;  // Track if hysteresis is satisfied
static int last_crossed_pole = -1;       // Track the last pole index where crossing occurred
double deadband;
double lastPoleChangeTime;
int incrementToSend = 0;
int decrementToSend = 0;
double angularDistance(double angle1, double angle2) {
  double diff = fabs(angle1 - angle2);
  return fmin(diff, 360.0 - diff);  // Handle wraparound
}

// Helper function to wrap pole index within range
int wrapPoleIndex(int pole_index) {
  while (pole_index < 0) {
    pole_index += NUM_POLES;
  }
  while (pole_index >= NUM_POLES) {
    pole_index -= NUM_POLES;
  }
  return pole_index;
}


double calculateDeadband() {
  double pole_distance = POLE_ANGLE_STEP;                 // Distance between poles in degrees
  double deadband = pole_distance * deadbandCoefficient;  // 10% of the pole distance
  double min_deadband = 1.0;                              // Minimum deadband = 1 degree
  return fmax(deadband, min_deadband);                    // Ensure deadband is at least 1 degree
}
// Global variables for continuous angle tracking
double continuousAngle = 0;
double lastRawAngle = 0;


// Update continuousAngle based on new raw reading
void updateContinuousAngle() {
  double currentRaw = getCurrentAngle();
  double delta = currentRaw - lastRawAngle;
  // Unwrap delta if needed
  if (delta > 180.0) delta -= 360.0;
  else if (delta < -180.0) delta += 360.0;
  continuousAngle += delta;
  lastRawAngle = currentRaw;
}

/*
void switchToPositionControl() {
  // target angles
  motor.velocity_limit = 5;  // [rad/s] cca 50rpm
  // open loop control config
  motor.controller = MotionControlType::angle_openloop;
  // init motor hardware
  motor.init();
}
*/

void switchToPositionControl() {
  motor.voltage_limit = 3;  // [V]
  // limit/set the velocity of the transition in between
  // target angles
  motor.velocity_limit = 3;  // [rad/s] cca 50rpm
  // target angles
  motor.velocity_limit = 5;  // [rad/s] cca 50rpm
  // open loop control config
  motor.controller = MotionControlType::angle_openloop;
  // init motor hardware
  motor.init();
}

void switchToTorqueControl() {
  // Switch the motor back to torque control mode
  motor.controller = MotionControlType::torque;
  // Optionally, reinitialize FOC (if needed)
  //motor.initFOC();
  motor.init();
  Serial.println("Switched back to torque control mode.");
}

// Helper function to normalize angles to 0-360 degrees
float normalizeAngle(float angle) {
  angle = fmod(angle, 360.0f);
  return angle < 0 ? angle + 360.0f : angle;
}

// Helper function to calculate smallest error considering wrapping
float calculateAngleError(float target, float current) {
  float error = target - current;
  error = fmod(error, 360.0f);
  if (error > 180.0f) error -= 360.0f;
  else if (error < -180.0f) error += 360.0f;
  return error;
}
/*
void goToPos(float target) {
  const float Kp = 1.0f;  // Proportional gain (adjust based on your system)
  const float tolerance = 1.5f;
  const unsigned long settleDuration = 100;

  target = normalizeAngle(target);
  unsigned long settleStartTime = 0;
  bool inToleranceZone = false;

  motor.enable();  // Enable motor at start

  while (true) {
    float current = getRawAngle(); 
    float error = calculateAngleError(target, current);
    float output = Kp * error;
    Serial.println("Angle error:" + String(error) + " - Control output:" + String(output));
    // Apply P control
    motor.move(output);
    motor.loopFOC();
    //Serial.print(output);

    // Check settling condition
    if (fabs(error) <= tolerance) {
      if (!inToleranceZone) {
        settleStartTime = millis();
        inToleranceZone = true;
      } else if (millis() - settleStartTime >= settleDuration) {
        break;  // Exit when settled for required duration
      }
    } else {
      inToleranceZone = false;
    }
  }

  motor.disable();  // Disable motor when settled
}
*/

/*
void moveToPosOpenLoop(float target_angle_deg) {
  // Switch to position control mode before beginning movement.
  switchToPositionControl();

  // Normalize the target to 0–360 degrees.
  target_angle_deg = normalizeAngle(target_angle_deg);

  // Use the sensor reading as the starting point for our commanded (absolute) position.
  // (We assume sensor.getSensorAngle() returns the shaft angle in degrees.)
  float cumulative_command_deg = normalizeAngle(sensor.getSensorAngle());

  // Set a tolerance (in degrees) for considering the target reached.
  const float tolerance = 2.0;
  unsigned long startTime = millis();

  // Loop until either the error is within tolerance or 3 seconds have passed.
  while (millis() - startTime < 5000) {
    // Read the current motor position from the sensor and normalize it.
    float current_deg = normalizeAngle(sensor.getSensorAngle());

    // Compute the error between the target and current position, handling wraparound.
    float error_deg = calculateAngleError(target_angle_deg, current_deg);
    Serial.print("Angle error:" + String(error_deg));
    // If the error is within tolerance, we consider the movement complete.
    if (fabs(error_deg) < tolerance) {
      break;
    }
    Serial.print(" | Target angle: " + String(target_angle_deg));
    Serial.print(" | Current angle: " + String(getCurrentAngle()));
    // Because open‐loop commands are cumulative, we need to command the motor
    // to change its setpoint by the difference between the new desired commanded
    // angle and the previously commanded angle.
    // Here, the desired commanded position is our current cumulative command plus the error.
    float desired_cumulative_deg = cumulative_command_deg + error_deg;
    Serial.print(" | Desired cumulative deg:" + String(desired_cumulative_deg));
    // The incremental change to command is then:
    float delta_deg = desired_cumulative_deg - cumulative_command_deg;
    float delta_rad = delta_deg * DEG_TO_RAD;
    Serial.print(" | delta deg:" + String(delta_deg));
    // Issue the incremental move (motor.move adds delta_rad to the previously commanded position).
    motor.move(delta_rad);

    // Update the cumulative commanded position.
    cumulative_command_deg += delta_deg;
    Serial.println(" | cumulative command deg:" + String(cumulative_command_deg));
    // Small delay to allow the motor to respond.
    delay(10);
    Serial.println("Timeout:" + String(getCurrentAngle()));
  }

  // After the loop (whether due to timeout or reaching the target),
  // switch back to torque control mode.
  Serial.println("Movement finished at " + String(getCurrentAngle()));
  switchToTorqueControl();
}
*/



void initEncoder() {
  Wire.end();
  Wire.setPins(21, 20);
  Wire.begin();
  Wire.setClock(400000);
  sensor.init();
  delay(100);
  //angle_offset = normalizeAngle(motor.shaftAngle() * (180.0 / PI));  // Convert to degrees and normalize
}

void initMotor() {
  motor.linkSensor(&sensor);
  driver.voltage_power_supply = 5;
  driver.pwm_frequency = 50000;
  driver.init();
  motor.linkDriver(&driver);
  motor.voltage_sensor_align = 2;
  motor.voltage_limit = motorVoltageLimit;
  motor.controller = MotionControlType::torque;
  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.init();
  motor.initFOC();
  //moveToPosOpenLoop(0.1);
}

void initHaptics() {
  deadband = calculateDeadband();
  if (NUM_POLES > 0) {
    POLE_ANGLE_STEP = 360.0 / NUM_POLES;
  } else {
    Serial.println("⚠️ Error: numAttractors must be greater than 0!");
    POLE_ANGLE_STEP = 0;
  }
  Serial.print("⚙️ POLE_ANGLE_STEP calculated: ");
  Serial.print(POLE_ANGLE_STEP);
  Serial.println("°");
  //previous_pole_index = wrapPoleIndex(round(angle_offset / POLE_ANGLE_STEP));
}

void initPID() {
  pidController.SetMode(AUTOMATIC);
  pidController.SetOutputLimits(-motor.voltage_limit, motor.voltage_limit);
  pidController.SetSampleTime(1);
}

void initHapticControl() {
  initPID();
  initEncoder();
  initMotor();
  initHaptics();
}

double HYSTERESIS_THRESHOLD = 1;

double lowPassFilter(double current_value, double previous_value, double alpha) {
  return alpha * current_value + (1 - alpha) * previous_value;
}



double getCurrentAngle() {
  filtered_angle = lowPassFilter(getRawAngle(), filtered_angle, alpha);
  current_angle = filtered_angle;
  //Serial.print("Angle:");
  //Serial.println(current_angle);
  return current_angle;
}

double getRawAngle() {
  raw_angle = normalizeAngle(motor.shaftAngle() * (180.0 / PI));
  current_angle = raw_angle;
  return raw_angle;  // return current angle in 0-360 degrees format.
}


double getClosestPoleAngle() {
  pole_index = wrapPoleIndex(floor((getRawAngle() + POLE_ANGLE_STEP / 2) / POLE_ANGLE_STEP));
  closest_pole_angle = pole_index * POLE_ANGLE_STEP;
  return closest_pole_angle;
}

void updateHapticControl() {
  motor.loopFOC();
  angle_error = getClosestPoleAngle() - getCurrentAngle();
  deadband = calculateDeadband();
  if (fabs(angle_error) > (deadband / 2.0)) {

    target_angle = getClosestPoleAngle();
    pidController.Compute();
    direction = (angle_error > 0) ? 1.0 : -1.0;
    if (target_voltage > 0.6) {
      motor.move(direction * fabs(target_voltage));
    }
  } else {
    //motor.move(0);
  }
  handleHaptics();
}

void handleHaptics() {
  // Detect pole crossing with hysteresis
  if (!crossed_hysteresis) {
    // Check if we are far enough from the pole to reset hysteresis
    if (angularDistance(filtered_angle, closest_pole_angle) > HYSTERESIS_THRESHOLD) {
      crossed_hysteresis = true;  // Enable crossing detection
    }
  } else {
    if ((last_angle < closest_pole_angle && filtered_angle >= closest_pole_angle) || (last_angle > closest_pole_angle && filtered_angle <= closest_pole_angle)) {

      // Only update if we've actually crossed to a new pole
      if (pole_index != last_crossed_pole) {
        // Calculate the difference between current and last pole indices
        int diff = pole_index - last_crossed_pole;
        // Handle wrapping: if the difference is more than half the number of poles,
        // adjust it to reflect the shorter wrap-around path.
        if (diff > NUM_POLES / 2) {
          diff -= NUM_POLES;
        } else if (diff < -NUM_POLES / 2) {
          diff += NUM_POLES;
        }

        // Use the calculated difference to update setPoint and the send counters
        if (diff > 0) {
          // Increase: add the number of poles crossed.
          incrementToSend += diff;
          int newSetPoint = setPoint + diff;
          if (newSetPoint > 100) {
            newSetPoint = 100;
          }
          setPoint = newSetPoint;
          Serial.println(setPoint);
          setIncreaseAnimate();
        } else if (diff < 0) {
          // Decrease: subtract the number of poles crossed.
          decrementToSend += -diff;           // (-diff) gives the absolute value.
          int newSetPoint = setPoint + diff;  // diff is negative here.
          if (newSetPoint < 0) {
            newSetPoint = 0;
          }
          setPoint = newSetPoint;
          setDecreaseAnimate();
        }
        motor.enable();
        lastPoleChangeTime = millis();
        last_crossed_pole = pole_index;  // Update last crossed pole
      }
      // Reset hysteresis after a crossing has been detected
      crossed_hysteresis = false;
    }
  }

  // Disable motor after a short period without pole change
  if (millis() - 10 > lastPoleChangeTime) {
    motor.disable();
  }

  handleIncrementDetrement();
  last_angle = filtered_angle;
  last_setPoint = setPoint;
}

/*
void handleIncrementDetrement() {
  if (incrementToSend > 0) {
    softSerial.print("+");
    incrementToSend--;
  }
  if (decrementToSend > 0) {
    softSerial.print("-");
    decrementToSend--;
  }
}
*/
void handleIncrementDetrement() {
  String toSend = "";
  if (incrementToSend > 0) {
    if (incrementToSend > 1) {
      toSend = "setpoint=" + String(setPoint);
      incrementToSend = 0;
    } else {
      toSend = "+";
      incrementToSend--;
    }
    Serial1.print(toSend);
    Serial.println("Sent to display:" + toSend);
  }
  if (decrementToSend > 0) {
    if (decrementToSend > 1) {
      toSend = "setpoint=" + String(setPoint);
      decrementToSend = 0;
    } else {
      toSend = "-";
      decrementToSend--;
    }
    Serial1.print(toSend);
    Serial.println("Sent to display:" + toSend);
  }
}

/*
void handleIncrementDetrement() {
  if ((millis() > lastUpdateSetpoint + setPointSendCooldown) && (setPoint != last_setPoint)) {
    String setPointString = "setpoint:" + String(setPoint);
    softSerial.print(setPointString);
    last_setPoint = setPoint;
    Serial.print("setPoint sent:");
    Serial.println(setPoint);
  }
}
*/
