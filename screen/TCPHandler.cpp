#include "TCPHandler.h"

TCPHandler::TCPHandler(WiFiHandler* wifiHdlr)
  : wifiHandler(wifiHdlr), tcpServer(nullptr), soundServerPort(12345), 
    lightServerPort(12345), hasNewTCPMessage(false) {
  
  soundServerIP = "";
  lightServerIP = "";
}

TCPHandler::~TCPHandler() {
  if (tcpServer) {
    tcpServer->stop();
    delete tcpServer;
    tcpServer = nullptr;
  }
}

void TCPHandler::startTCPServer(int port) {
  // Clean up any existing server
  if (tcpServer) {
    tcpServer->stop();
    delete tcpServer;
    tcpServer = nullptr;
  }
  
  // Create and start a new server
  tcpServer = new WiFiServer(port);
  tcpServer->begin();
  
  Serial.println("TCP server started on port " + String(port));
}

void TCPHandler::handleTCPServer() {
  if (!tcpServer) {
    return;
  }
  
  // Check if there's a new client connection
  if (tcpServer->hasClient()) {
    // If we already have a client, disconnect it
    if (tcpServerClient) {
      tcpServerClient.stop();
      Serial.println("Previous client disconnected");
    }

    // Accept the new client
    tcpServerClient = tcpServer->available();
    Serial.println("New client connected");

    // Send welcome message
    tcpServerClient.println("ESP32 Configuration Server");
    tcpServerClient.println("Type 'help' for available commands");
    if (wifiHandler->isAPModeActive()) {
      tcpServerClient.println("IP: " + WiFi.softAPIP().toString());
    } else if (wifiHandler->isWiFiConnected()) {
      tcpServerClient.println("IP: " + WiFi.localIP().toString());
    }
  }

  // Only check for data if client exists and is connected
  if (!tcpServerClient || !tcpServerClient.connected()) {
    return;
  }

  // Check if data is available once - avoids repeated function calls
  int availableBytes = tcpServerClient.available();
  if (availableBytes <= 0) {
    return;
  }

  // Process up to 3 commands per call to avoid blocking too long
  int commandsProcessed = 0;
  while (tcpServerClient.available() > 0 && commandsProcessed < 3) {
    // Use readStringUntil but with a timeout to avoid blocking
    String command = tcpServerClient.readStringUntil('\n');
    command.trim();

    if (command.length() > 0) {
      Serial.println("Received TCP command: " + command);
      tcpReceivedCommand = command;
      hasNewTCPMessage = true;

      // Echo the command back to the client
      tcpServerClient.println("Received: " + command);
      
      commandsProcessed++;
    }
    
    // Small yield to allow other processing
    yield();
  }
}

bool TCPHandler::getHasNewMessage() {
  return hasNewTCPMessage;
}

String TCPHandler::getMessage() {
  hasNewTCPMessage = false;
  return tcpReceivedCommand;
}

void TCPHandler::sendTCPResponse(const String& message) {
  if (tcpServerClient && tcpServerClient.connected()) {
    tcpServerClient.println(message);
    Serial.println("TCP Response: " + message);
  }
}

bool TCPHandler::connectToServer(const String& ip, uint16_t port) {
  if (!wifiHandler->isWiFiConnected()) {
    Serial.println("Cannot connect to server: WiFi not connected");
    return false;
  }
  
  if (ip.isEmpty()) {
    Serial.println("Cannot connect to server: IP address is empty");
    return false;
  }

  Serial.println("Connecting to server at: " + ip + ":" + String(port));
  
  // Try to connect with timeout
  unsigned long startTime = millis();
  while (!client.connect(ip.c_str(), port)) {
    if (millis() - startTime > 5000) {  // 5-second timeout
      Serial.println("Server connection timed out.");
      return false;
    }
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("\nServer connected.");
  return true;
}

bool TCPHandler::connectToSoundServer() {
  return connectToServer(soundServerIP, soundServerPort);
}

bool TCPHandler::connectToLightServer() {
  return connectToServer(lightServerIP, lightServerPort);
}

void TCPHandler::sendData(const String& data) {
  if (client.connected()) {
    Serial.println("Sending data: " + data);
    client.print(data + "\n");
  } else {
    Serial.println("Error: Client is not connected.");
  }
}

String TCPHandler::receiveData() {
  if (client.connected() && client.available()) {
    String data = client.readStringUntil('\n');
    data.trim();
    Serial.println("Received data: " + data);
    return data;
  }
  return "";
}

void TCPHandler::disconnect() {
  if (client.connected()) {
    client.stop();
  }
}

bool TCPHandler::isServerConnected() {
  return client.connected();
}

void TCPHandler::setServerInfo(const String& soundIP, uint16_t soundPort, 
                              const String& lightIP, uint16_t lightPort) {
  soundServerIP = soundIP;
  soundServerPort = soundPort;
  lightServerIP = lightIP;
  lightServerPort = lightPort;
  
  Serial.println("TCP Server Info updated:");
  Serial.println("  Sound Server: " + soundIP + ":" + String(soundPort));
  Serial.println("  Light Server: " + lightIP + ":" + String(lightPort));
}

// Replace the update() method in TCPHandler.cpp with this optimized version
void TCPHandler::update() {
  // If there's no server, nothing to do
  if (!tcpServer) return;
  
  // Only check for new client if we don't already have one
  if (!tcpServerClient || !tcpServerClient.connected()) {
    if (tcpServer->hasClient()) {
      // Accept the new client
      tcpServerClient = tcpServer->available();
      Serial.println("New client connected");

      // Send welcome message
      tcpServerClient.println("ESP32 Configuration Server");
      tcpServerClient.println("Type 'help' for available commands");
      
      // Only get IP once, don't call WiFi functions multiple times
      if (wifiHandler->isAPModeActive()) {
        static IPAddress lastAPIP = WiFi.softAPIP();
        tcpServerClient.println("IP: " + lastAPIP.toString());
      } else if (wifiHandler->isWiFiConnected()) {
        static IPAddress lastIP = WiFi.localIP();
        tcpServerClient.println("IP: " + lastIP.toString());
      }
    }
  } else {
    // Fast check for available data
    int availableBytes = tcpServerClient.available();
    if (availableBytes > 0) {
      // Read one command per cycle to avoid blocking
      String command = tcpServerClient.readStringUntil('\n');
      command.trim();

      if (command.length() > 0) {
        tcpReceivedCommand = command;
        hasNewTCPMessage = true;

        // Echo the command back to the client
        tcpServerClient.println("Received: " + command);
      }
    }
  }
}