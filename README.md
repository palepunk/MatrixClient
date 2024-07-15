# MatrixClient

A client library for interacting with Matrix servers using an ESP32.

## Installation

### Using PlatformIO

1. Open your PlatformIO project.
2. Add the following line to your `platformio.ini` file: lib_deps = https://github.com/plugdio/MatrixClient


### Manual Installation

1. Clone this repository or download the ZIP file.
2. Copy the `MatrixClient` directory to your `libraries` folder in your Arduino or PlatformIO project.

## Usage

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "MatrixClient.h"

const char* ssid = "your-SSID";
const char* password = "your-PASSWORD";
const char* authorizedUserId = "@authorized_user:matrix.org";
const char* matrixUser = "your-matrix-username";
const char* matrixPassword = "your-matrix-password";
const char* defaultServerHost = "matrix.org";

WiFiClientSecure client;
MatrixClient matrixClient(client);

void setup() {
 Serial.begin(115200);
 WiFi.begin(ssid, password);

 while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
     matrixClient.logger("Connecting to WiFi...");
 }

 matrixClient.logger("Connected to WiFi");

 matrixClient.setMasterUserId(authorizedUserId);

 if (matrixClient.login(matrixUser, matrixPassword, defaultServerHost)) {
     matrixClient.logger("Login successful!");
     if (matrixClient.sendDMToMaster("The client is now online.", "m.notice")) {
         matrixClient.logger("Message sent successfully!");
     } else {
         matrixClient.logger("Failed to send message.");
     }
 } else {
     matrixClient.logger("Login failed.");
 }
}

void loop() {
 if (matrixClient.sync()) {
     matrixClient.logger("Synced successfully");
 } else {
     matrixClient.logger("Failed to sync");
 }

 std::vector<MatrixEvent> events = matrixClient.getRecentEvents();
 for (const MatrixEvent& event : events) {
     matrixClient.logger("Event ID: " + event.eventId);
     matrixClient.logger("Event Type: " + event.eventType);
     matrixClient.logger("Sender: " + event.sender);
     matrixClient.logger("Room ID: " + event.roomId);
     matrixClient.logger("Room Name: " + event.roomName);
     matrixClient.logger("Room Topic: " + event.roomTopic);
     matrixClient.logger("Room Encryption: " + String(event.roomEncryption));
     matrixClient.logger("Message Type: " + event.messageType);
     matrixClient.logger("Message Content: " + event.messageContent);
     matrixClient.logger("-------------------");
 }

 delay(1000);  // Delay to simulate periodic checks
}

