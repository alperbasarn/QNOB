#include "MQTTHandler.h"

MQTTHandler::MQTTHandler(WiFiHandler* wifiHdlr, EEPROMManager* eepromMgr)
  : wifiHandler(wifiHdlr), eepromManager(eepromMgr), mqttClient(wifiClient),
    mqtt_port(8883), lastMQTTSentTime(0), lastMQTTConnectAttempt(0), useLightConfig(false) {
  
  mqtt_broker = "";
  mqtt_username = "";
  mqtt_password = "";
  
  // Set buffer size for MQTT packets
  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
  
  // Setup callback using a static function
  mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->callback(topic, payload, length);
  });
  
  // Skip certificate validation for development
  wifiClient.setInsecure();
}

MQTTHandler::~MQTTHandler() {
  // No dynamic resources to clean
}

void MQTTHandler::callback(char* topic, byte* payload, unsigned int length) {
  // Only process messages if we have internet connectivity
  if (!wifiHandler->isInternetAvailable()) {
    return;
  }

  Serial.print("📥 Received MQTT Message on topic: ");
  Serial.println(topic);

  // Safely handle payload
  char messageBuffer[256] = {0};
  unsigned int copyLength = length < sizeof(messageBuffer) - 1 ? length : sizeof(messageBuffer) - 1;
  
  // Copy payload to buffer
  memcpy(messageBuffer, payload, copyLength);
  messageBuffer[copyLength] = '\0'; // Null terminate
  
  Serial.println("📩 Message Content: " + String(messageBuffer));

  // Handle specific topics
  if (String(topic) == "esp32/light/brightness") {
    int brightness = atoi(messageBuffer);
    int waterLevel = (brightness * 100) / 255;
    Serial.println("Received Brightness via MQTT: " + String(waterLevel));
  }
}

void MQTTHandler::logMQTTState(int state) {
  switch (state) {
    case -4: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECTION_TIMEOUT"); break;
    case -3: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECTION_LOST"); break;
    case -2: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECT_FAILED"); break;
    case -1: Serial.println("⚠️ MQTT Connection failed: MQTT_DISCONNECTED"); break;
    case 0: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECTED"); break;
    case 1: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECT_BAD_PROTOCOL"); break;
    case 2: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECT_BAD_CLIENT_ID"); break;
    case 3: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECT_UNAVAILABLE"); break;
    case 4: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECT_BAD_CREDENTIALS"); break;
    case 5: Serial.println("⚠️ MQTT Connection failed: MQTT_CONNECT_UNAUTHORIZED"); break;
    default: Serial.println("⚠️ MQTT Connection failed: Unknown error"); break;
  }
}

void MQTTHandler::initializeMQTT(bool useLightConfig) {
  // Safety check before trying to access EEPROM manager
  if (!eepromManager) {
    Serial.println("Error: EEPROM manager is null in initializeMQTT");
    return;
  }

  // Store which configuration we're using
  this->useLightConfig = useLightConfig;

  // Load the MQTT configuration from eepromManager based on flag
  // IMPORTANT: Always load configuration even without internet
  if (useLightConfig) {
    mqtt_broker = eepromManager->lightMQTTServerURL;
    mqtt_port = eepromManager->lightMQTTServerPort;
    mqtt_username = eepromManager->lightMQTTUsername;
    mqtt_password = eepromManager->lightMQTTPassword;
    Serial.println("Using Light MQTT configuration");
  } else {
    mqtt_broker = eepromManager->soundMQTTServerURL;
    mqtt_port = eepromManager->soundMQTTServerPort;
    mqtt_username = eepromManager->soundMQTTUsername;
    mqtt_password = eepromManager->soundMQTTPassword;
    Serial.println("Using Sound MQTT configuration");
  }

  // Check for empty values to avoid crashes
  if (mqtt_broker.isEmpty()) {
    Serial.println("Warning: MQTT broker URL is empty");
    return;
  }

  // Set default port if invalid
  if (mqtt_port <= 0 || mqtt_port > 65535) {
    mqtt_port = 8883;
  }

  // Print configuration
  Serial.println("=== MQTT Configuration ===");
  Serial.println("Broker: " + mqtt_broker);
  Serial.println("Port: " + String(mqtt_port));
  Serial.println("Username: " + mqtt_username);
  Serial.println("Password length: " + String(mqtt_password.length()));
  Serial.println("==========================");
  
  // Set the server (don't connect yet)
  mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
  
  Serial.println("MQTT client initialized successfully");
  
  // Only try to connect immediately if we have internet
  if (wifiHandler->isInternetAvailable()) {
    Serial.println("Internet available, attempting MQTT connection");
    connectToMQTTServer();
  } else {
    Serial.println("No internet connectivity. MQTT connection will be attempted when internet is available.");
  }
}

bool MQTTHandler::connectToMQTTServer() {
  // Skip MQTT connection if no internet connectivity
  if (!wifiHandler->isInternetAvailable()) {
    Serial.println("⚠️ No internet connectivity. Skipping MQTT connection.");
    return false;
  }

  // Avoid frequent connection attempts
  unsigned long currentMillis = millis();
  if (currentMillis - lastMQTTConnectAttempt < MQTT_RECONNECT_DELAY) {
    return false;
  }
  lastMQTTConnectAttempt = currentMillis;

  // Check if MQTT client is already connected
  if (mqttClient.connected()) {
    Serial.println("✅ Already connected to MQTT Broker.");
    return true;
  }

  // Validate MQTT configuration
  if (mqtt_broker.isEmpty()) {
    Serial.println("⚠️ MQTT Broker URL is not configured. Skipping connection.");
    return false;
  }

  Serial.print("🔄 Attempting MQTT connection to: ");
  Serial.println(mqtt_broker + ":" + String(mqtt_port));

  // Debug memory available before connection attempt
  Serial.println("Free heap before MQTT connect: " + String(esp_get_free_heap_size()));

  // Make sure device name isn't empty
  String deviceNameStr = "ESP32";
  if (eepromManager && !eepromManager->deviceName.isEmpty()) {
    deviceNameStr = eepromManager->deviceName;
  }

  // Create a safe client ID (shorter and more reliable)
  char clientId[30] = {0};
  snprintf(clientId, sizeof(clientId), "ESP32-%s-%04X", 
           deviceNameStr.substring(0, 8).c_str(), 
           (uint16_t)random(0xFFFF));

  Serial.print("Client ID: ");
  Serial.println(clientId);

  // Convert String values to char arrays to avoid potential memory issues
  char usernameBuf[MAX_USERNAME_LENGTH] = {0};
  char passwordBuf[MAX_PASS_LENGTH] = {0};
  
  if (!mqtt_username.isEmpty()) {
    mqtt_username.toCharArray(usernameBuf, sizeof(usernameBuf));
  }
  if (!mqtt_password.isEmpty()) {
    mqtt_password.toCharArray(passwordBuf, sizeof(passwordBuf));
  }

  Serial.print("MQTT username: ");
  Serial.println(usernameBuf);
  Serial.println("Setting timeout for connection attempt...");
  
  // Only try once to avoid freezing if there are connectivity issues
  bool connectResult = false;
  
  // Safely attempt the connection
  connectResult = mqttClient.connect(clientId, 
                                    mqtt_username.isEmpty() ? nullptr : usernameBuf,
                                    mqtt_password.isEmpty() ? nullptr : passwordBuf);
  
  // If connection fails, we'll just try again later
  if (!connectResult) {
    // Check internet connectivity again to be sure
    bool internetNowAvailable = wifiHandler->checkInternetConnectivity();
    
    if (internetNowAvailable) {
      // If we still have internet, it was another type of connection failure
      int state = mqttClient.state();
      Serial.print("\n⚠️ Failed to connect to MQTT Broker, state: ");
      Serial.println(state);
      logMQTTState(state);
    } else {
      Serial.println("\n⚠️ Internet connectivity lost during MQTT connection attempt.");
    }
    return false;
  }

  // Connection succeeded
  Serial.println("\n✅ Successfully connected to MQTT Broker!");
  
  // Subscribe to topics
  mqttClient.subscribe("ledRing/modeControl");
  mqttClient.subscribe("esp32/ledRing/configure");
  mqttClient.subscribe("ledRing/brightnessControl");
  mqttClient.subscribe("ledRing/colorControl");
  mqttClient.subscribe("esp32/sound/control");
  
  Serial.println("📩 Subscribed to MQTT topics");
  return true;
}

void MQTTHandler::sendMQTTMessage(const char* topic, const String& message) {
  // Skip if no internet or MQTT not configured
  if (!wifiHandler->isInternetAvailable() || mqtt_broker.isEmpty()) {
    // Log that we would have sent a message, but didn't due to no internet
    Serial.print("⚠️ Not sending MQTT message (no internet/broker) - Topic: ");
    Serial.print(topic);
    Serial.print(", Message: ");
    Serial.println(message);
    return;
  }

  unsigned long currentMillis = millis();
  if (mqttClient.connected() && (currentMillis - lastMQTTSentTime >= 100)) {
    // Convert message to char array
    char messageBuffer[256] = {0};
    if (message.length() < sizeof(messageBuffer) - 1) {
      message.toCharArray(messageBuffer, sizeof(messageBuffer));
      mqttClient.publish(topic, messageBuffer);
      Serial.println("📤 Sent MQTT message on topic [" + String(topic) + "]: " + message);
      lastMQTTSentTime = currentMillis;
    } else {
      Serial.println("⚠️ MQTT message too long (max 255 chars)");
    }
  } else if (!mqttClient.connected()) {
    // Let the user know we couldn't send due to not being connected
    Serial.println("⚠️ MQTT client not connected, queuing reconnection");
  }
}

bool MQTTHandler::hasSoundMQTTConfigured() {
  if (!eepromManager) return false;
  return !eepromManager->soundMQTTServerURL.isEmpty();
}

bool MQTTHandler::hasLightMQTTConfigured() {
  if (!eepromManager) return false;
  return !eepromManager->lightMQTTServerURL.isEmpty();
}

bool MQTTHandler::isMQTTConnected() {
  return wifiHandler->isInternetAvailable() && mqttClient.connected();
}

void MQTTHandler::update() {
  // Get current time once
  unsigned long currentMillis = millis();
  
  // Skip MQTT processing if there's no internet, but don't clear the configuration
  if (!wifiHandler->isInternetAvailable()) {
    // If we were connected but now internet is gone, disconnect cleanly
    if (mqttClient.connected()) {
      Serial.println("Internet connectivity lost, MQTT will disconnect");
      mqttClient.disconnect();
    }
    return;
  }
  
  // Only attempt MQTT operations if broker URL is configured
  if (!mqtt_broker.isEmpty()) {
    if (!mqttClient.connected()) {
      // Only try to connect once per minute to avoid blocking other operations
      if (currentMillis - lastMQTTConnectAttempt >= MQTT_RECONNECT_DELAY) {
        Serial.println("Internet available, attempting periodic MQTT connection");
        connectToMQTTServer();
        lastMQTTConnectAttempt = currentMillis;
      }
    } else {
      // We're connected, process incoming MQTT messages
      // This is non-blocking and should be called regularly
      mqttClient.loop();
    }
  } else if (currentMillis - lastMQTTConnectAttempt >= MQTT_RECONNECT_DELAY) {
    // If the broker URL is empty but we have internet, try initializing again
    // This handles the case where MQTT wasn't properly initialized on startup
    initializeMQTT(useLightConfig);
    lastMQTTConnectAttempt = currentMillis;
  }
}