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
const char* defaultServerHost = "matrix-client.matrix.org";

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