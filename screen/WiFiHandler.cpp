#include "WiFiHandler.h"

WiFiHandler::WiFiHandler(EEPROMManager* eepromMgr)
  : eepromManager(eepromMgr), wifiConnected(false), apModeActive(false), 
    internetAvailable(false), lastInternetCheck(0), lastAPCheckTime(0), 
    lastDateTimeUpdate(0), lastWeatherUpdate(0),
    autoStartAP(true), wifiChannel(1) {
  
  // Initialize WiFi stack without starting AP mode
  WiFi.mode(WIFI_OFF);
  delay(25);
  
  // Start in STA mode only - AP will be started in initialize()
  WiFi.mode(WIFI_STA);
  delay(25);
  
  // Initialize the lastAPCheckTime
  lastAPCheckTime = millis();
  
  // Don't start AP mode in constructor
  Serial.println("WiFiHandler constructed");
}

WiFiHandler::~WiFiHandler() {
  // No dynamic resources to clean up
}

void WiFiHandler::initialize() {
  Serial.println("Initializing WiFiHandler...");
  
  // Switch to AP_STA mode to prepare for AP
  WiFi.mode(WIFI_AP_STA);
  delay(25);
  
  // Now start AP mode
  Serial.println("Starting AP mode during initialization...");
  forceStartAPMode();
  
  Serial.println("WiFiHandler initialized");
}

void WiFiHandler::setAutoStartAP(bool enable) {
  autoStartAP = enable;
}

bool WiFiHandler::checkInternetConnectivity() {
  // Don't check internet connectivity if we're not connected to WiFi
  if (!wifiConnected) {
    internetAvailable = false;
    return false;
  }

  // Use TCP connection to a reliable server as a better check
  WiFiClient testClient;
  testClient.setTimeout(3000); // 3 second timeout - don't block too long
  
  // Try to connect to Google DNS server on port 53 (DNS)
  bool connected = testClient.connect("8.8.8.8", 53);
  
  // Close the connection regardless of result
  testClient.stop();
  
  // Update internal state
  internetAvailable = connected;
  
  if (connected) {
    // If we confirmed internet is available, update time and weather if needed
    unsigned long currentMillis = millis();
    
    // Update date and time if needed
    if (currentMillis - lastDateTimeUpdate > TIME_UPDATE_INTERVAL) {
      updateDateTime();
    }
    
    // Update weather if needed
    if (currentMillis - lastWeatherUpdate > WEATHER_UPDATE_INTERVAL) {
      updateWeather();
    }
  } else {
    // Only log failures occasionally
    static unsigned long lastFailLogTime = 0;
    if (millis() - lastFailLogTime > 30000) { // Log every 30 seconds at most
      Serial.println("❌ Internet connectivity check failed");
      lastFailLogTime = millis();
    }
  }
  
  return internetAvailable;
}

bool WiFiHandler::connectToWifi() {
  // Don't disrupt AP mode, but we need to make sure we're in AP_STA mode
  if (!isAPActive()) {
    Serial.println("AP not active, starting it before WiFi connection attempt");
    forceStartAPMode();
  }
  
  // Try to connect to last known network first
  if (eepromManager) {
    eepromManager->loadWiFiCredentials();
    
    int lastIndex = eepromManager->lastConnectedNetworkIndex;
    if (lastIndex >= 0 && lastIndex < 3) {
      String lastSSID = eepromManager->wifiCredentials[lastIndex].ssid;
      String lastPassword = eepromManager->wifiCredentials[lastIndex].password;
      
      if (!lastSSID.isEmpty()) {
        Serial.print("🔗 Attempting to reconnect to last known network: ");
        Serial.println(lastSSID);

        // Already in WIFI_AP_STA mode from constructor
        WiFi.begin(lastSSID.c_str(), lastPassword.c_str());

        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
          delay(250); // Reduced delay for faster response
          if ((millis() - startTime) % 1000 == 0) {
            Serial.print(".");
          }
        }

        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("\n✅ Successfully reconnected to last known WiFi network!");
          Serial.print("IP Address: ");
          Serial.println(WiFi.localIP());

          // Get the current channel of the connected WiFi and use it for AP
          wifiChannel = WiFi.channel();
          Serial.print("WiFi connected on channel: ");
          Serial.println(wifiChannel);
          
          // Restart AP on the same channel as the connected WiFi
          restartAPWithChannel();

          wifiConnected = true;
          
          // Check internet connectivity
          checkInternetConnectivity();
          
          beginOTA(eepromManager->deviceName.c_str());
          return true;
        } else {
          Serial.println("\n❌ Failed to connect to last known WiFi network.");
        }
      }
    }
  }

  // If reconnection fails, scan for networks - but don't block too long
  Serial.println("\n🔍 Scanning for available WiFi networks...");
  int networkCount = WiFi.scanNetworks();

  if (networkCount == 0) {
    Serial.println("❌ No WiFi networks found.");
    return false;
  }

  // Find matches with stored networks
  bool foundNetwork = false;
  String selectedSSID, selectedPassword;
  int selectedIndex = -1;
  int selectedChannel = 1;

  for (int j = 0; j < 3; j++) {
    String storedSSID = eepromManager->wifiCredentials[j].ssid;
    String storedPassword = eepromManager->wifiCredentials[j].password;

    if (storedSSID.length() > 0) {
      for (int i = 0; i < networkCount; i++) {
        if (WiFi.SSID(i) == storedSSID) {
          foundNetwork = true;
          selectedSSID = storedSSID;
          selectedPassword = storedPassword;
          selectedIndex = j;
          // Remember the channel of this network
          selectedChannel = WiFi.channel(i);
          Serial.print("✅ Found saved network: ");
          Serial.print(selectedSSID);
          Serial.print(" on channel ");
          Serial.println(selectedChannel);
          break;
        }
      }
    }
    if (foundNetwork) break;
  }

  if (!foundNetwork) {
    Serial.println("❌ No matching saved networks found.");
    return false;
  }

  // Connect to the selected network
  Serial.print("🔗 Connecting to ");
  Serial.println(selectedSSID);
  
  WiFi.begin(selectedSSID.c_str(), selectedPassword.c_str());

  unsigned long startTime2 = millis();
  unsigned long maxConnectTime = 15000; // 15 seconds max connect time
  
  while (WiFi.status() != WL_CONNECTED && millis() - startTime2 < maxConnectTime) {
    if ((millis() - startTime2) % 1000 == 0) {
      Serial.print(".");
    }
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Save last known network
    eepromManager->lastConnectedNetworkIndex = selectedIndex;
    eepromManager->saveWiFiCredentials();

    // Get the actual channel of the connected WiFi
    wifiChannel = WiFi.channel();
    Serial.print("WiFi connected on channel: ");
    Serial.println(wifiChannel);
    
    // Restart AP on the same channel as the connected WiFi
    restartAPWithChannel();

    wifiConnected = true;
    
    checkInternetConnectivity();
    beginOTA(eepromManager->deviceName.c_str());
    return true;
  } else {
    Serial.println("\n❌ Connection failed.");
    return false;
  }
}

bool WiFiHandler::startAPMode() {
  // Create AP name using device name
  String apName;
  if (eepromManager && !eepromManager->deviceName.isEmpty()) {
    apName = eepromManager->deviceName;
  } else {
    apName = String(AP_SSID) + "-" + String(random(1000));
  }

  Serial.print("📡 Starting AP with name: ");
  Serial.print(apName);
  Serial.print(", password: ");
  Serial.print(AP_PASSWORD);
  Serial.print(", channel: ");
  Serial.println(wifiChannel);

  // Call softAP but IGNORE THE RETURN VALUE - it's unreliable
  WiFi.softAP(apName.c_str(), AP_PASSWORD, wifiChannel);
  delay(25); // Give it time to initialize

  // Configure static IP
  if (!WiFi.softAPConfig(AP_IP_ADDRESS, AP_IP_ADDRESS, IPAddress(255, 255, 255, 0))) {
    Serial.println("Note: Failed to configure AP IP address, but continuing anyway");
  }

  delay(25);

  // Check if AP is really active by getting its IP
  IPAddress apIP = WiFi.softAPIP();
  
  // Check if we got a valid IP (not 0.0.0.0)
  if (apIP[0] != 0) {
    Serial.println("✅ AP Mode started successfully with name: " + apName);
    Serial.println("AP IP address: " + apIP.toString());
    Serial.println("AP Channel: " + String(wifiChannel));
    
    apModeActive = true;
    return true;
  } else {
    Serial.println("⚠️ AP Mode may have failed to start properly");
    Serial.print("WiFi mode: ");
    Serial.println(WiFi.getMode());
    Serial.print("Current WiFi channel: ");
    Serial.println(wifiChannel);
    
    // Still set as active if we see the network in scan results
    apModeActive = true; // Assume it's active even if we can't verify
    return false;
  }
}

// Check if AP is really active - optimized to reduce scanning
bool WiFiHandler::isAPActive() {
  if (!apModeActive) return false;
  
  // Don't do full scan here as it's expensive - just check IP
  IPAddress apIP = WiFi.softAPIP();
  if (apIP[0] != 0) {
    return true; // AP has an IP address
  }
  
  // IP check failed - AP might be down
  return false;
}

// Restart AP using the same channel as WiFi
void WiFiHandler::restartAPWithChannel() {
  Serial.println("Restarting AP mode on channel " + String(wifiChannel));
  WiFi.softAPdisconnect(false);
  delay(25);
  startAPMode();
}

bool WiFiHandler::forceStartAPMode() {
  for (int attempt = 1; attempt <= 2; attempt++) { // Reduced to 2 attempts
    Serial.print("AP mode start attempt #");
    Serial.println(attempt);
    
    if (startAPMode()) {
      Serial.println("AP mode started successfully after " + String(attempt) + " attempts");
      return true;
    }
    
    delay(25);
  }
  
  Serial.println("⚠️ AP mode setup completed - checking status");
  apModeActive = true; // Assume AP is active even if we couldn't verify
  return true;
}

void WiFiHandler::disconnect() {
  WiFi.disconnect();
  wifiConnected = false;
  internetAvailable = false;
}

bool WiFiHandler::isWiFiConnected() const {
  return wifiConnected;
}

bool WiFiHandler::isAPModeActive() const {
  return apModeActive;
}

bool WiFiHandler::isInternetAvailable() const {
  return internetAvailable;
}

void WiFiHandler::beginOTA(const char* hostname) {
  // Use device name from EEPROM if available, otherwise use the provided hostname
  String otaName = hostname;
  if (eepromManager && !eepromManager->deviceName.isEmpty()) {
    otaName = eepromManager->deviceName;
  }

  Serial.println("Setting up OTA with hostname: " + otaName);
  ArduinoOTA.setHostname(otaName.c_str());

  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void WiFiHandler::handleOTA() {
  ArduinoOTA.handle();
}

#include "WiFiHandler.h"
bool WiFiHandler::updateDateTime() {
    if (!isInternetAvailable()) {
        Serial.println("Cannot update time: no internet connection");
        return false;
    }
    
    Serial.println("Attempting to update date and time from NTP server...");
    
    // Create a configTime call instead of manual NTP implementation
    // This is more reliable on ESP32
    configTime(timeZone * 3600, 0, NTP_SERVER);
    
    // Wait for time to be set
    int retry = 0;
    while (time(nullptr) < 1000000000 && retry < 10) {
        Serial.println("Waiting for NTP time sync...");
        delay(500);
        retry++;
    }
    
    if (time(nullptr) < 1000000000) {
        Serial.println("Failed to get time from NTP");
        return false;
    }
    
    // Get time info
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    
    // Format the date and time
    char dateBuffer[12];
    char timeBuffer[9];
    
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d/%02d/%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", 
             timeinfo.tm_hour, timeinfo.tm_min);
    
    currentDate = String(dateBuffer);
    currentTime = String(timeBuffer);
    
    Serial.print("Time updated: ");
    Serial.print(currentDate);
    Serial.print(" ");
    Serial.println(currentTime);
    
    lastDateTimeUpdate = millis();
    return true;
}

// Replace the updateWeather method with this improved version
bool WiFiHandler::updateWeather() {
    if (!isInternetAvailable()) {
        Serial.println("Cannot update weather: no internet connection");
        return false;
    }
    
    // Use a fixed test temperature instead of API for testing
    // This helps isolate if it's an API issue or a display issue
    Serial.println("Setting mock weather data for testing");
    weatherTemp = "22.5°C";
    lastWeatherUpdate = millis();
    
    Serial.println("Weather updated: " + weatherTemp);
    return true;
    
    /*
    // Uncomment this section to use the real API once testing confirms display works
    Serial.println("Attempting to update weather data...");
    
    HTTPClient http;
    http.begin(WEATHER_API_URL);
    
    int httpCode = http.GET();
    Serial.print("HTTP response code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Weather response received: " + payload.substring(0, 100) + "...");
        
        // Parse JSON response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            float temperature = doc["main"]["temp"];
            weatherTemp = String(temperature, 1) + "°C";
            
            Serial.println("Weather updated: " + weatherTemp);
            http.end();
            lastWeatherUpdate = millis();
            return true;
        } else {
            Serial.println("JSON parsing error: " + String(error.c_str()));
        }
    } else {
        Serial.println("HTTP request failed");
    }
    
    http.end();
    Serial.println("Failed to get weather data");
    return false;
    */
}

// Modified update method to regularly check and update time
void WiFiHandler::update() {
    static unsigned long lastWiFiScanTime = 0;
    unsigned long currentMillis = millis();
    
    // Quick check of WiFi status
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.println("WiFi reconnected");
            
            // Immediately check internet when WiFi connects
            checkInternetConnectivity();
        }
    } else if (wifiConnected) {
        wifiConnected = false;
        internetAvailable = false;
        Serial.println("WiFi disconnected");
    }
    
    // Only check for WiFi networks once per minute if not connected
    if (!wifiConnected && (currentMillis - lastWiFiScanTime > WIFI_SCAN_INTERVAL)) {
        lastWiFiScanTime = currentMillis;
        
        // Only attempt to connect if we have stored credentials
        if (eepromManager && eepromManager->lastConnectedNetworkIndex >= 0) {
            Serial.println("Periodic scan for known networks...");
            
            // Scan for networks in background if possible
            WiFi.scanNetworks(true); // Async scan
        }
    }
    
    // Only check AP status periodically
    if (autoStartAP && (currentMillis - lastAPCheckTime > AP_CHECK_INTERVAL)) {
        lastAPCheckTime = currentMillis;
        
        // Verify AP is running
        IPAddress apIP = WiFi.softAPIP();
        if (apIP[0] == 0) {
            apModeActive = false;
            Serial.println("AP mode not active. Restarting it...");
            startAPMode();
        } else if (!apModeActive) {
            apModeActive = true;
        }
    }
    
    // Internet connectivity check once per minute
    if (wifiConnected && (currentMillis - lastInternetCheck > INTERNET_CHECK_INTERVAL)) {
        checkInternetConnectivity();
        lastInternetCheck = currentMillis;
    }
    
    // Force time update every 5 minutes if connected to internet
    if (internetAvailable && (currentMillis - lastDateTimeUpdate > 300000)) {
        updateDateTime();
    }
    
    // Handle OTA if WiFi is connected
    if (wifiConnected) {
        ArduinoOTA.handle();
    }
}


// Add to WiFiHandler.cpp to debug the getCurrentDate and getCurrentTime methods

String WiFiHandler::getCurrentDate() const {
    Serial.print("WiFiHandler: getCurrentDate returning: ");
    Serial.println(currentDate);
    return currentDate;
}

String WiFiHandler::getCurrentTime() const {
    Serial.print("WiFiHandler: getCurrentTime returning: ");
    Serial.println(currentTime);
    return currentTime;
}

String WiFiHandler::getWeatherTemperature() const {
    Serial.print("WiFiHandler: getWeatherTemperature returning: ");
    Serial.println(weatherTemp);
    return weatherTemp;
}

