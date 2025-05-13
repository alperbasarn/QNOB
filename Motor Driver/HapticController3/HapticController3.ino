#include <SimpleFOC.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <PID_v1.h>
#include <ArduinoOTA.h>  // OTA Library

// Define hardware serial pins
#define SERIAL_RX_PIN 4
#define SERIAL_TX_PIN 3

BLDCMotor motor = BLDCMotor(8);
int NUM_POLES = 120;

float motorVoltageLimit = 2.5;
float motorVelocityLimit = 20;
float deadbandCoefficient = 0.1;

int brightness = 0;
float volumePercent = 0;
int currentColor = 1;  // RGBW
int demoMode = 1;      //Windows volume control, 2 fusion 360 rotation
int currentLED = 0;
int lastUpdate = millis();
int lastUpdateClient = millis();
int lastUpdateSetpoint = millis();
int setPointSendCooldown = 25;
int setPoint = 0;
int last_setPoint = 0;
float ledVal = 1;
int proxRead = 0;
int rRead = 0, gRead = 0, bRead = 0;
int pidSampleTime = 10;
// PID parameters (tune these values for your application)
double Kp = 0.5, Ki = 0.0f, Kd = 0.0f;
// Target and current angles
double target_angle = 0;    // Desired angle setpoint (changed to double)
double current_angle = 0;   // Current angle from the sensor (changed to double)
double target_voltage = 0;  // Output of the PID controller (changed to double)
double direction;
// PID controller instance
PID pidController(&current_angle, &target_voltage, &target_angle, Kp, Ki, Kd, DIRECT);
//velocityPID pidController(&current_angle, &target_velocity, &target_angle, Kp, Ki, Kd, DIRECT);

void setup() {
  Serial.begin(115200);
  initializeSoftSerial();  // We'll keep the function name for simplicity
  initHapticControl();
  initLED();
  Serial.println("init complete");
}

void loop() {
  updateHapticControl();
  updateLED();
  updateSerial();
  updateSoftSerial();  // We'll keep the function name for simplicity
}