#ifndef TCP_HANDLER_H
#define TCP_HANDLER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include "WiFiHandler.h"

class TCPHandler {
private:
  WiFiHandler* wifiHandler;            // Reference to WiFi handler
  WiFiClient client;                   // TCP client for server connections
  WiFiServer* tcpServer = nullptr;     // TCP server for AP mode
  WiFiClient tcpServerClient;          // Current TCP server client connection
  String tcpReceivedCommand;           // Buffer for received TCP command
  bool hasNewTCPMessage = false;       // Flag for new TCP message
  
  // Server parameters
  String soundServerIP;
  uint16_t soundServerPort;
  String lightServerIP;
  uint16_t lightServerPort;
  
public:
  TCPHandler(WiFiHandler* wifiHandler);
  ~TCPHandler();

  // TCP server methods
  void startTCPServer(int port);       // Start TCP server on specified port
  void handleTCPServer();              // Handle incoming TCP connections and data
  bool getHasNewMessage();             // Check if there's a new TCP message
  String getMessage();                 // Get the received TCP message
  void sendTCPResponse(const String& message); // Send response to connected TCP client
  
  // TCP client methods
  bool connectToServer(const String& ip, uint16_t port); // Connect to TCP server
  bool connectToSoundServer();         // Connect to configured sound server
  bool connectToLightServer();         // Connect to configured light server
  void sendData(const String& data);   // Send data to connected server
  String receiveData();                // Receive data from connected server
  void disconnect();                   // Disconnect from TCP server
  bool isServerConnected();            // Check if connected to TCP server
  
  // Configuration methods
  void setServerInfo(const String& soundIP, uint16_t soundPort, 
                    const String& lightIP, uint16_t lightPort);
  
  // Main update method
  void update();                       // Handle periodic TCP tasks
};

#endif // TCP_HANDLER_H