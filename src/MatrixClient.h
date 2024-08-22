#ifndef MATRIX_CLIENT_H
#define MATRIX_CLIENT_H

#include <functional>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Client.h>
#include <vector>

struct MatrixEvent {
    String eventId;
    String eventType;
    String sender;
    String roomId;
    String roomName;
    String roomTopic;
    bool roomEncryption = false;
    String messageType;
    String messageContent;
};

enum LogLevel {
    ERROR,
    INFO,
    DEBUG
};
typedef void (*LoggerFunction)(LogLevel, const String& message);

class MatrixClient {
public:
    using LoggerFunction = std::function<void(LogLevel, const String&)>;
    MatrixClient(Client& client, LoggerFunction logger = nullptr);
    bool login(const String& matrixUser, const String& matrixPassword, const String& defaultServerHost);
    void setMasterUserId(const String& userId);
    bool sendDMToMaster(const String& message, const String& msgType = "m.text");
    bool sync();
    bool createRoom(const String& userId, String& roomId);
    bool joinRoom(const String& roomId);
    bool sendReadReceipt(const String& roomId, const String& eventId);
    bool sendMessageToRoom(const String& roomId, const String& message, const String& msgType = "m.text");
    bool sendMediaToRoom(const String& roomId, const String& fileName, const String& contentType, const uint8_t* fileData, size_t fileSize);
    std::vector<MatrixEvent> getRecentEvents();

    int syncTimeout = 5000; // The maximum time to wait, in milliseconds, before server responds to the sync request.
    unsigned int waitForResponse = 1000;
    int maxMessageLength = 1500;

    static LogLevel logLevel; // Global log level setting

private:
    String performHTTPRequest(const String& url, const String& method, const String& payload, bool useAuth = true);
    bool readHTTPResponse(String &body, String &headers);
    bool discoverServer(const String& matrixUser);
    String extractJsonBody(const String& responseBody);
    bool extractNextBatch(const String& responseBody);
    bool ensureAccessToken();
    bool refreshAccessToken();
    void storeEvent(const MatrixEvent& event);
    String uploadMedia(const String& fileName, const String& contentType, const uint8_t* fileData, size_t fileSize);
    

    Client *client;
    LoggerFunction logger;
    String homeserverUrl;
    String accessToken;
    String refreshToken;
    String syncToken;
    String masterUserId;
    String masterRoomId;
    unsigned long tokenExpiryTime;
    std::vector<MatrixEvent> recentEvents;
    static void defaultLoggerFunction(LogLevel level, const String& message) {
        if (level <= logLevel) {
            Serial.println(message);
        }
    }
};

#endif // MATRIX_CLIENT_H
