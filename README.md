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
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "MatrixClient.h"

const char* ssid = "your-SSID";
const char* password = "your-PASSWORD";
const char* authorizedUserId = "@authorized_user:matrix.org";
const char* matrixUser = "your-matrix-username";
const char* matrixPassword = "your-matrix-password";
const char* defaultServerHost = "matrix-client.matrix.org";  // if server discovery is not working

WiFiClientSecure client;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

void logger(LogLevel level, const String& message) {
    if (level <= MatrixClient::logLevel) {
        timeClient.update();
        unsigned long epochTime = timeClient.getEpochTime();
        unsigned long hours = (epochTime % 86400L) / 3600;
        unsigned long minutes = (epochTime % 3600) / 60;
        unsigned long seconds = epochTime % 60;

        char timeBuffer[16];
        snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);

        String logMessage = "[" + String(timeBuffer) + "] ";
        switch (level) {
            case ERROR:
                logMessage += "[ERROR] ";
                break;
            case INFO:
                logMessage += "[INFO] ";
                break;
            case DEBUG:
                logMessage += "[DEBUG] ";
                break;
        }
        logMessage += message;
        Serial.println(logMessage);
    }
}

MatrixClient matrixClient(client, logger);

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("Connected to WiFi");

    timeClient.begin();
    while (!timeClient.update()) {
        timeClient.forceUpdate();
    }

    client.setInsecure();

    matrixClient.setMasterUserId(authorizedUserId);
    MatrixClient::logLevel = DEBUG;

    if (matrixClient.login(matrixUser, matrixPassword, defaultServerHost)) {
        logger(INFO, "Login successful!");
        if (matrixClient.sendDMToMaster("The client is now online.", "m.notice")) {
            logger(DEBUG, "Message sent successfully!");
        } else {
            logger(ERROR, "Failed to send message.");
        }
    } else {
        logger(ERROR, "Login failed.");
    }
}

void loop() {
    if (matrixClient.sync()) {
        logger(DEBUG, "Synced successfully");
    } else {
        logger(ERROR, "Failed to sync");
    }

    std::vector<MatrixEvent> events = matrixClient.getRecentEvents();
    for (const MatrixEvent& event : events) {
        logger(INFO, "Event ID: " + event.eventId);
        logger(INFO, "Event Type: " + event.eventType);
        logger(INFO, "Sender: " + event.sender);
        logger(INFO, "Room ID: " + event.roomId);
        logger(INFO, "Room Name: " + event.roomName);
        logger(INFO, "Room Topic: " + event.roomTopic);
        logger(INFO, "Room Encryption: " + String(event.roomEncryption));
        logger(INFO, "Message Type: " + event.messageType);
        logger(INFO, "Message Content: " + event.messageContent);
        logger(INFO, "-------------------");

        if (event.eventType == "invitation" && !event.roomEncryption && event.sender == authorizedUserId) {
            matrixClient.joinRoom(event.roomId);
        }

        if (event.eventType == "message" && event.sender == authorizedUserId) {
            matrixClient.sendReadReceipt(event.roomId, event.eventId);
            matrixClient.sendMessageToRoom(event.roomId, "Unknown command");
        }
    }

    delay(1000);  // Delay to simulate periodic checks
}
```
## Logging Levels
MatrixClient supports three levels of logging:

* ERROR: Logs critical issues that need immediate attention.
* INFO: Logs general information about the operation of the program.
* DEBUG: Logs detailed debugging information.

### Setting Log Level
The global log level can be set by modifying the logLevel variable in the MatrixClient class. The default log level is INFO.