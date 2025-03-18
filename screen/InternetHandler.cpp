#include "InternetHandler.h"
#include "WiFiHandler.h"
#include "TimeHandler.h"
#include "WeatherHandler.h"

InternetHandler::InternetHandler(WiFiHandler* wifiHandler)
  : wifiHandler(wifiHandler), internetAvailable(false), lastInternetCheck(0) {

  // Create sub-handlers once WiFiHandler reference is set
  timeHandler = new TimeHandler(this);
  weatherHandler = new WeatherHandler(this);

  Serial.println("InternetHandler initialized");
}

InternetHandler::~InternetHandler() {
  delete timeHandler;
  delete weatherHandler;
}

bool InternetHandler::checkInternetConnectivity() {
  // Don't check internet connectivity if we're not connected to WiFi
  if (!wifiHandler || !wifiHandler->isWiFiConnected()) {
    Serial.println("DEBUG: Cannot check internet - WiFi not connected");
    internetAvailable = false;
    return false;
  }

  Serial.println("DEBUG: Checking internet connectivity...");

  // Use TCP connection to a reliable server as a better check
  WiFiClient testClient;
  testClient.setTimeout(3000);  // 3 second timeout - don't block too long

  // Try to connect to Google DNS server on port 53 (DNS)
  bool connected = testClient.connect("8.8.8.8", 53);

  // Close the connection regardless of result
  testClient.stop();

  // Update internal state
  bool previousState = internetAvailable;
  internetAvailable = connected;

  // Always log the current state for debugging
  Serial.print("DEBUG: Internet connectivity check result: ");
  Serial.println(internetAvailable ? "CONNECTED" : "DISCONNECTED");

  // Log change of state
  if (previousState != internetAvailable) {
    if (internetAvailable) {
      Serial.println("✅ Internet connectivity established");
    } else {
      Serial.println("❌ Internet connectivity lost");
    }
  }

  lastInternetCheck = millis();
  return internetAvailable;
}

// New delegation methods
bool InternetHandler::updateDateTime() {
  if (!internetAvailable) {
    Serial.println("DEBUG: Cannot update time - No internet connection");
    return false;
  }

  Serial.println("DEBUG: Calling TimeHandler updateDateTime");
  return timeHandler ? timeHandler->updateDateTime() : false;
}

bool InternetHandler::updateWeather() {
  if (!internetAvailable) {
    Serial.println("DEBUG: Cannot update weather - No internet connection");
    return false;
  }

  Serial.println("DEBUG: Calling WeatherHandler updateWeather");
  return weatherHandler ? weatherHandler->updateWeather() : false;
}

void InternetHandler::update() {
  unsigned long currentMillis = millis();

  // Check internet connectivity periodically
  if (currentMillis - lastInternetCheck >= INTERNET_CHECK_INTERVAL) {
    checkInternetConnectivity();
    // Update sub-handlers if we have internet
    if (internetAvailable) {
      if (timeHandler) {
        timeHandler->update();
      } else {
        Serial.println("ERROR: timeHandler is NULL");
      }
      if (weatherHandler) {
        if (currentMillis - lastWeatherCheck >= WEATHER_CHECK_INTERVAL) weatherHandler->update();
      } else {
        Serial.println("ERROR: weatherHandler is NULL");
      }
    } else {
      // Log this periodically but not on every update
      static unsigned long lastNoInternetLog = 0;
      if (currentMillis - lastNoInternetLog >= 10000) {  // Log every 10 seconds
        Serial.println("DEBUG: InternetHandler update - No internet, skipping updates");
        lastNoInternetLog = currentMillis;
      }
    }
  }
}

// Delegation methods for backward compatibility
String InternetHandler::getCurrentDate() const {
  return timeHandler ? timeHandler->getCurrentDate() : "----/--/--";
}

String InternetHandler::getCurrentTime() const {
  return timeHandler ? timeHandler->getCurrentTime() : "--:--";
}

String InternetHandler::getDayOfWeek() const {
  return timeHandler ? timeHandler->getDayOfWeek() : "---";
}

String InternetHandler::getWeatherTemperature() const {
  return weatherHandler ? weatherHandler->getWeatherTemperature() : "--°C";
}