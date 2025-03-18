#include "TimeHandler.h"
#include "InternetHandler.h"
#include <time.h>

TimeHandler::TimeHandler(InternetHandler* internet, int timeZone)
  : internetHandler(internet), timeZone(timeZone),
    timeClient(nullptr), timeUpdated(false), lastNTPSync(0) {

  // DO NOT initialize NTP client in constructor
  Serial.println("TimeHandler initialized");
}

TimeHandler::~TimeHandler() {
  if (timeClient) {
    timeClient->end();
    delete timeClient;
  }
}

String TimeHandler::getWeekdayString(int wday) {
  // Convert TimeLib weekday (1-7) to our 3-letter format (SUN, MON, etc.)
  static const char* days[] = { "???", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" }; // Index 0 is invalid
  
  if (wday >= 1 && wday <= 7) {
    return days[wday];
  } else {
    return "???";  // Invalid weekday
  }
}

bool TimeHandler::updateDateTime() {
  if (!internetHandler || !internetHandler->isInternetAvailable()) {
    Serial.println("Cannot update time: no internet connection");
    return false;
  }

  // Lazy initialization of NTP client
  if (!timeClient) {
    Serial.println("Initializing NTP client");
    timeClient = new NTPClient(ntpUDP, NTP_SERVER);
    timeClient->setTimeOffset(timeZone * 3600);  // Convert hours to seconds
    timeClient->begin();
  }

  Serial.println("Attempting to update date and time from NTP server...");

  // Force update from NTP server
  bool ntpUpdateSuccess = timeClient->update();

  if (!ntpUpdateSuccess) {
    Serial.println("Failed to update time from NTP server");
    // Retry a few times
    for (int i = 0; i < 3; i++) {
      delay(500);
      if (timeClient->update()) {
        ntpUpdateSuccess = true;
        break;
      }
    }

    if (!ntpUpdateSuccess) {
      return false;
    }
  }

  // Get current epoch time
  time_t epochTime = timeClient->getEpochTime();
  
  // Set the TimeLib time - this starts the internal clock running
  setTime(epochTime);
  
  // Mark time as updated and store sync time
  timeUpdated = true;
  lastNTPSync = millis();
  
  // Update our string variables from the new time
  updateTimeVariables();
  
  Serial.print("Time updated from NTP: ");
  Serial.print(currentDate);
  Serial.print(" ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.println(dayOfWeek);

  lastDateTimeUpdate = millis();
  return true;
}

void TimeHandler::updateTimeVariables() {
  // Format the date as YYYY/MM/DD
  char dateBuffer[11];
  sprintf(dateBuffer, "%04d/%02d/%02d", year(), month(), day());
  currentDate = String(dateBuffer);

  // Format the time as HH:MM
  char timeBuffer[6];
  sprintf(timeBuffer, "%02d:%02d", hour(), minute());
  currentTime = String(timeBuffer);
  
  // Get day of week using TimeLib's weekday() function
  dayOfWeek = getWeekdayString(weekday());
}

String TimeHandler::getCurrentDate() const {
  return currentDate;
}

String TimeHandler::getCurrentTime() const {
  return currentTime;
}

String TimeHandler::getDayOfWeek() const {
  return dayOfWeek;
}

bool TimeHandler::isTimeValid() const {
  return timeUpdated;
}

void TimeHandler::update() {
  unsigned long currentMillis = millis();
  
  // Check if we need to resync with NTP server (every 5 minutes)
  if (timeUpdated && (currentMillis - lastNTPSync > NTP_RESYNC_INTERVAL)) {
    Serial.println("5 minutes passed, resetting timeUpdated to trigger NTP resync");
    timeUpdated = false;
  }
  
  // If time is not updated or we need to resync, try to update from NTP
  if (!timeUpdated) {
    updateDateTime();
  }
  // Otherwise, if it's time for a local update, update time variables from TimeLib
  else if (currentMillis - lastDateTimeUpdate > TIME_UPDATE_INTERVAL) {
    updateTimeVariables();
    lastDateTimeUpdate = currentMillis;
  }
}