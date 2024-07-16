#ifndef MATRIX_CLIENT_H
#define MATRIX_CLIENT_H

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

class MatrixClient {
public:
    MatrixClient(Client &client);
    bool login(const String& matrixUser, const String& matrixPassword, const String& defaultServerHost);
    void setMasterUserId(const String& userId);
    bool sendDMToMaster(const String& message, const String& msgType = "m.text");
    bool sync();
    bool createRoom(const String& userId, String& roomId);
    bool joinRoom(const String& roomId);
    bool sendReadReceipt(const String& roomId, const String& eventId);
    bool sendMessageToRoom(const String& roomId, const String& message, const String& msgType = "m.text");
    std::vector<MatrixEvent> getRecentEvents();
    void logger(const String& message);

    int syncTimeout = 5000; // The maximum time to wait, in milliseconds, before server responds to the sync request.
    unsigned int waitForResponse = 1000;
    int maxMessageLength = 1500;

private:
    String performHTTPRequest(const String& url, const String& method, const String& payload, bool useAuth = true);
    bool readHTTPResponse(String &body, String &headers);
    bool discoverServer(const String& matrixUser);
    String extractJsonBody(const String& responseBody);
    bool extractNextBatch(const String& responseBody);
    bool ensureAccessToken();
    bool refreshAccessToken();
    void storeEvent(const MatrixEvent& event);

    Client *client;
    String homeserverUrl;
    String accessToken;
    String refreshToken;
    String syncToken;
    String masterUserId;
    String masterRoomId;
    unsigned long tokenExpiryTime;
    std::vector<MatrixEvent> recentEvents;
};

#endif // MATRIX_CLIENT_H
