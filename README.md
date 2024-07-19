# MatrixClient

A client library for interacting with Matrix servers using an ESP32.

## What is Matrix?

Matrix is an open standard for decentralized communication. It is a protocol that enables secure, real-time communication over the internet. Matrix allows users to send messages, share files, and participate in voice and video calls. It is designed to be open and interoperable, allowing communication across different services and platforms.

## Supported Matrix Protocol Version and Methods

This library implements parts of version 3 (v3) of the Matrix client-server API. The following methods have been implemented:

### Authentication

- **Server Discovery**: partly implemented to find the homeserer based on the username. Fallback is using the defaultServerHost
- **Login**: Authenticate with the Matrix server using a username and password to obtain an access and refresh token.

### Messaging

- **Send Direct Message**: Send a direct message to a specified user.
- **Send Message to Room**: Send a message to a specified room.
- **Send Read Receipt**: Send a read receipt for a specific event in a room.

### Room Management

- **Create Room**: Create a new room and invite users.
- **Join Room**: Join an existing room by its room ID.

### Synchronization

- **Sync**: Synchronize the client's state with the server, receiving updates on messages, invitations, and other events. Only invitations and unencrypted messages are handled after the client has connected. Previous and other type of events are ignored.

## Installation

### Using PlatformIO

1. Open your PlatformIO project.
2. Add the following line to your `platformio.ini` file: lib_deps = https://github.com/palepunk/MatrixClient.git


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
const char* defaultServerHost = "matrix.org"; // if server discovery is not working

WiFiClientSecure client;
MatrixClient matrixClient(client);

void setup() {
 Serial.begin(115200);
 WiFi.begin(ssid, password);

 while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
     matrixClient.logger("Connecting to WiFi...");
 }
 client.setInsecure();
 
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

    if(event.eventType == "invitation" && !event.roomEncryption && event.sender == authorizedUserId) {
        matrixClient.joinRoom(event.roomId);
    }

    if(event.eventType == "message" && event.sender == authorizedUserId) {
        matrixClient.sendReadReceipt(event.roomId, event.eventId);
        matrixClient.sendMessageToRoom(event.roomId, "Unknown command");
    }
 }
 
 delay(1000);  // Delay to simulate periodic checks
}

