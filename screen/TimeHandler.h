// TimeHandler.h
#ifndef TIME_HANDLER_H
#define TIME_HANDLER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

// Forward declaration
class InternetHandler;

class TimeHandler {
private:
    InternetHandler* internetHandler;  // Reference to parent InternetHandler
    WiFiUDP ntpUDP;                    // UDP instance for NTP
    NTPClient* timeClient;             // NTP Client for time synchronization
    
    String currentDate = "----/--/--"; // Current date (default value)
    String currentTime = "--:--";      // Current time (default value)
    String dayOfWeek = "---";          // Current day of week (default value)
    
    unsigned long lastDateTimeUpdate = 0; // Time of last date/time update
    unsigned long lastNTPSync = 0;     // Time of last successful NTP sync
    
    const int timeZone;                // Timezone offset in hours
    bool timeUpdated;                  // Flag to indicate if time is valid
    
    // Configuration values
    static const unsigned long TIME_UPDATE_INTERVAL = 1000; // 1 second for local updates
    static const unsigned long NTP_RESYNC_INTERVAL = 300000; // 5 minutes for NTP resync
    static constexpr const char* NTP_SERVER = "pool.ntp.org";
    
    // Convert TimeLib weekday (1-7) to our 3-letter format (SUN, MON, etc.)
    String getWeekdayString(int wday);
    
    // Method to update time variables from TimeLib
    void updateTimeVariables();

public:
    /**
     * Constructor
     * 
     * @param internet Pointer to the InternetHandler object
     * @param timeZone Time zone offset in hours (default: 9)
     */
    TimeHandler(InternetHandler* internet, int timeZone = 9);
    
    /**
     * Destructor - cleans up NTP client if created
     */
    ~TimeHandler();

    /**
     * Updates date and time from NTP server
     * 
     * @return True if time was successfully updated, false otherwise
     */
    bool updateDateTime();
    
    /**
     * Gets the current date string in YYYY/MM/DD format
     * 
     * @return Current date string
     */
    String getCurrentDate() const;
    
    /**
     * Gets the current time string in HH:MM format
     * 
     * @return Current time string
     */
    String getCurrentTime() const;
    
    /**
     * Gets the current day of week as a 3-letter string (e.g., "MON")
     * 
     * @return Current day of week string
     */
    String getDayOfWeek() const;
    
    /**
     * Checks if the time has been successfully updated at least once
     * 
     * @return True if time is valid, false otherwise
     */
    bool isTimeValid() const;
    
    // TimeLib wrapper functions for direct access to time components
    /**
     * Gets the current year
     * 
     * @return Current year (4-digit)
     */
    int getCurrentYear() { return year(); }
    
    /**
     * Gets the current month
     * 
     * @return Current month (1-12)
     */
    int getCurrentMonth() { return month(); }
    
    /**
     * Gets the current day of month
     * 
     * @return Current day (1-31)
     */
    int getCurrentDay() { return day(); }
    
    /**
     * Gets the current hour
     * 
     * @return Current hour (0-23)
     */
    int getCurrentHour() { return hour(); }
    
    /**
     * Gets the current minute
     * 
     * @return Current minute (0-59)
     */
    int getCurrentMinute() { return minute(); }
    
    /**
     * Gets the current second
     * 
     * @return Current second (0-59)
     */
    int getCurrentSecond() { return second(); }
    
    /**
     * Gets the current epoch time (seconds since Jan 1, 1970)
     * 
     * @return Current epoch time
     */
    time_t getCurrentEpochTime() { return now(); }
    
    /**
     * Gets the current weekday according to TimeLib
     * 
     * @return Current weekday (1-7, where 1=Sunday, 7=Saturday)
     */
    int getCurrentWeekday() { return weekday(); }
    
    /**
     * Update method - should be called regularly in loop()
     * Checks if time needs to be updated and performs updates as needed
     */
    void update();
};

#endif // TIME_HANDLER_H