#include "MatrixClient.h"
#include <WiFiClientSecure.h>

#define ZERO_COPY(STR)    ((char*)STR.c_str())

MatrixClient::MatrixClient(Client &client) {
    this->client = &client;
}

bool MatrixClient::discoverServer(const String& matrixUser) {
    int colonIndex = matrixUser.indexOf(':');
    if (colonIndex == -1) {
        logger("Invalid Matrix ID");
        return false;
    }

    String hostname = matrixUser.substring(colonIndex + 1);
    String url = "https://" + hostname + "/.well-known/matrix/client";

    String responseBody = performHTTPRequest(url, "GET", "", false);

    DynamicJsonDocument doc(maxMessageLength);
    DeserializationError error = deserializeJson(doc, ZERO_COPY(responseBody));
    if (!error) {
        if (doc.containsKey("m.homeserver") && doc["m.homeserver"].containsKey("base_url")) {
            homeserverUrl = doc["m.homeserver"]["base_url"].as<String>();
            logger("Discovered server URL: " + homeserverUrl);
            return true;
        } else {
            logger("No m.homeserver or base_url found in response");
        }
    } else {
        logger("deserializeJson() failed: ");
        logger(error.c_str());
        logger("responseBody: " + responseBody);
    }

    return false;
}

bool MatrixClient::login(const String& matrixUser, const String& matrixPassword, const String& defaultServerHost) {
    if (!discoverServer(matrixUser)) {
        homeserverUrl = "https://" + defaultServerHost;
        logger("Using default server URL: " + homeserverUrl);
    }

    // Generate unique device ID using the ESP32's MAC address
    uint64_t mac = ESP.getEfuseMac();
    char device_id[13];
    snprintf(device_id, sizeof(device_id), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);

    DynamicJsonDocument id(maxMessageLength);
    id["type"] = "m.id.user";
    id["user"] = matrixUser;

    DynamicJsonDocument req(maxMessageLength);
    req["type"] = "m.login.password";
    req["identifier"] = id;
    req["password"] = matrixPassword;
    req["device_id"] = device_id;
    req["refresh_token"] = true;

    String payload;
    serializeJson(req, payload);

    String responseBody = performHTTPRequest(homeserverUrl + "/_matrix/client/v3/login", "POST", payload, false);

    DynamicJsonDocument doc(maxMessageLength);
    DeserializationError error = deserializeJson(doc, ZERO_COPY(responseBody));
    if (!error) {
        if (doc.containsKey("access_token")) {
            accessToken = doc["access_token"].as<String>();
            if (doc.containsKey("refresh_token")) {
                refreshToken = doc["refresh_token"].as<String>();
                logger("Got the refresh token: " + refreshToken);
            }
            if (doc.containsKey("expires_in_ms")) {
                tokenExpiryTime = millis() + doc["expires_in_ms"].as<unsigned long>();
                logger("Access token expires in: " + String(doc["expires_in_ms"].as<unsigned long>()) + " ms");
            }
            logger("Got the access token: " + accessToken);
            return true;
        } else {
            logger("No access token found in response");
        }
    } else {
        Serial.print("deserializeJson() failed: ");
        logger(error.c_str());
        logger("responseBody: " + responseBody);
    }

    return false;
}

bool MatrixClient::sync() {
    if (!ensureAccessToken()) {
        logger("Cannot sync: failed to ensure access token");
        return false;
    }

    String url = homeserverUrl + "/_matrix/client/v3/sync";
    if (!syncToken.isEmpty()) {
        url += "?since=" + syncToken;
        if (syncTimeout > 0) {
            url += "&timeout=" + String(syncTimeout);
        }
    }

    String responseBody = performHTTPRequest(url, "GET", "");

    if (!extractNextBatch(responseBody)) {
//        logger("Next batch not found - sync");
    }

    DynamicJsonDocument doc(maxMessageLength * 2);  // Increased size to handle larger sync responses
    DeserializationError error = deserializeJson(doc, ZERO_COPY(responseBody));
    if (!error) {
        if (!syncToken.isEmpty()) { // don't processess the initial sync
            // Process events
            JsonObject rooms = doc["rooms"].as<JsonObject>();
            JsonObject join = rooms["join"].as<JsonObject>();
            JsonObject invite = rooms["invite"].as<JsonObject>();

            for (JsonPair kv : join) {
                String roomId = kv.key().c_str();
                JsonObject room = kv.value().as<JsonObject>();
                JsonArray events = room["timeline"]["events"].as<JsonArray>();
                for (JsonObject event : events) {
                    if (event["type"] == "m.room.message") {
                        MatrixEvent matrixEvent;
                        matrixEvent.eventId = event["event_id"].as<String>();
                        matrixEvent.eventType = "message";
                        matrixEvent.sender = event["sender"].as<String>();
                        matrixEvent.roomId = roomId;
                        matrixEvent.roomName = room["name"] | "";
                        matrixEvent.roomTopic = room["topic"] | "";
                        matrixEvent.roomEncryption = room.containsKey("encrypted");
                        matrixEvent.messageType = event["content"]["msgtype"].as<String>();
                        matrixEvent.messageContent = event["content"]["body"].as<String>();

                        storeEvent(matrixEvent);
                    }
                }
            }

            for (JsonPair kv : invite) {
                String roomId = kv.key().c_str();
                JsonObject room = kv.value().as<JsonObject>();
                JsonArray events = room["invite_state"]["events"].as<JsonArray>();
                MatrixEvent matrixEvent;
                matrixEvent.eventType = "invitation";
                matrixEvent.roomId = roomId;
                for (JsonObject event : events) {
                    if (event["type"] == "m.room.name") {
                        matrixEvent.roomName = event["content"]["name"].as<String>();
                    }
                    if (event["type"] == "m.room.topic") {
                        matrixEvent.roomTopic = event["content"]["topic"].as<String>();
                    }
                    if (event["type"] == "m.room.encryption") {
                        matrixEvent.roomEncryption = true;
                    }
                    if (event.containsKey("event_id")) {
                        matrixEvent.eventId = event["event_id"].as<String>();
                        matrixEvent.sender = event["sender"].as<String>();
                    }
                }
                storeEvent(matrixEvent);
            }
        }

    } else {
//        Serial.print("sync deserializeJson() failed: ");
//        logger(error.c_str());
//        logger("sync responseBody: " + responseBody);
    }

    return true;
}

bool MatrixClient::refreshAccessToken() {
    DynamicJsonDocument req(maxMessageLength);
    req["refresh_token"] = refreshToken;

    String payload;
    serializeJson(req, payload);

    String responseBody = performHTTPRequest(homeserverUrl + "/_matrix/client/v3/refresh", "POST", payload);
    DynamicJsonDocument doc(maxMessageLength);
    DeserializationError error = deserializeJson(doc, ZERO_COPY(responseBody));
    if (!error) {
        if (doc.containsKey("access_token")) {
            accessToken = doc["access_token"].as<String>();
            logger("Got the access token: " + accessToken);
            if (doc.containsKey("refresh_token")) {
                refreshToken = doc["refresh_token"].as<String>();
                logger("Got the refresh token: " + refreshToken);
                if (doc.containsKey("expires_in_ms")) {
                    tokenExpiryTime = millis() + doc["expires_in_ms"].as<unsigned long>();
                    logger("Access token refreshed. New expiry in: " + String(doc["expires_in_ms"].as<unsigned long>()) + " ms");
                }
                return true;

            }
        } else {
            logger("No access token found in response");
            logger(responseBody);
            logger("You should log in again!");
        }
    } else {
        Serial.print("refresh deserializeJson() failed: ");
        logger(error.c_str());
        return false;
    }
    return false;
}

bool MatrixClient::ensureAccessToken() {
    if (millis() >= tokenExpiryTime - 10000) {
        logger("Access token expired, refreshing...");
        return refreshAccessToken();
    }
    return true;
}

void MatrixClient::setMasterUserId(const String& userId) {
    masterUserId = userId;
}

bool MatrixClient::sendDMToMaster(const String& message, const String& msgType) {
    if (masterUserId.isEmpty()) {
        logger("Master user has not been set yet");
        return false;
    }

    if (!createRoom(masterUserId, masterRoomId)) {
        logger("Failed to create master room");
        return false;
    } else {
        return sendMessageToRoom(masterRoomId, message, msgType);
    }
}

bool MatrixClient::createRoom(const String& userId, String& roomId) {
    if (!ensureAccessToken()) {
        logger("Cannot create room: failed to ensure access token");
        return false;
    }

    StaticJsonDocument<256> req;
    req["invite"] = JsonArray();
    req["invite"].add(userId);
    req["is_direct"] = true;
    req["preset"] = "trusted_private_chat";

    String payload;
    serializeJson(req, payload);

    String responseBody = performHTTPRequest(homeserverUrl + "/_matrix/client/v3/createRoom", "POST", payload);

    DynamicJsonDocument doc(maxMessageLength);
    DeserializationError error = deserializeJson(doc, ZERO_COPY(responseBody));

    if (!error) {
        if (doc.containsKey("room_id")) {
            roomId = doc["room_id"].as<String>();
            logger("Room created: " + roomId);
            return true;
        } else {
            logger("No room_id found in response");
        }
    } else {
        Serial.print("createRoom deserializeJson() failed: ");
        logger(error.c_str());
        logger(responseBody);
    }

    return false;
}

bool MatrixClient::sendMessageToRoom(const String& roomId, const String& message, const String& msgType) {
    if (!ensureAccessToken()) {
        logger("Cannot send message: failed to ensure access token");
        return false;
    }

    StaticJsonDocument<256> req;
    req["msgtype"] = msgType;
    req["body"] = message;

    String payload;
    serializeJson(req, payload);

    String url = homeserverUrl + "/_matrix/client/v3/rooms/" + roomId + "/send/m.room.message/" + String(millis());
    String responseBody = performHTTPRequest(url, "PUT", payload);

    DynamicJsonDocument doc(maxMessageLength);
    DeserializationError error = deserializeJson(doc, ZERO_COPY(responseBody));

    if (!error) {
        if (doc.containsKey("event_id")) {
            logger("Message sent to room: " + roomId);
            return true;
        } else {
            logger("No event_id found in response");
            logger(responseBody);
        }
    } else {
        Serial.print("sendMessageToRoom deserializeJson() failed: ");
        logger(error.c_str());
        logger(responseBody);
    }

    return false;
}

bool MatrixClient::joinRoom(const String& roomId) {
    if (!ensureAccessToken()) {
        logger("Cannot join room: failed to ensure access token");
        return false;
    }
    String url = homeserverUrl + "/_matrix/client/v3/join/" + roomId;
    String responseBody = performHTTPRequest(url, "POST", "");
    return true;
}

bool MatrixClient::sendReadReceipt(const String& roomId, const String& eventId) {
    if (!ensureAccessToken()) {
        logger("Cannot send read reciept: failed to ensure access token");
        return false;
    }
    String url = homeserverUrl + "/_matrix/client/v3/rooms/" + roomId + "/receipt/m.read/" + eventId;
    String responseBody = performHTTPRequest(url, "POST", "");
    return true;
}

String MatrixClient::performHTTPRequest(const String& url, const String& method, const String& payload, bool useAuth) {
    String host;
    String path;
    const int httpsPort = 443;

    // Parse URL
    int index = url.indexOf("://");
    if (index == -1) {
        logger("Invalid URL");
        return "";
    }

    String remainder = url.substring(index + 3);

    index = remainder.indexOf('/');
    if (index == -1) {
        logger("Invalid URL");
        return "";
    }
    host = remainder.substring(0, index);
    path = remainder.substring(index);

    if (!client->connect(host.c_str(), httpsPort)) {
        logger("Connection to " + host + " failed");
        return "";
    }

    // Create HTTP request
    client->print(method + " " + path + " HTTP/1.1\r\n");
    client->print("Host: " + host + "\r\n");
    client->print("User-Agent: ESP32\r\n");
    client->print("Content-Type: application/json\r\n");
    if (useAuth) {
        client->print("Authorization: Bearer " + accessToken + "\r\n");
    }
    if (method != "GET") {
        client->print("Content-Length: " + String(payload.length()) + "\r\n");
    }
    client->print("\r\n");
    if (method != "GET") {
        client->print(payload);
    }

    String responseBody, headers;
    readHTTPResponse(responseBody, headers);
    client->stop();

 //   logger("HTTP " + method + " request to " + url + " completed with response: " + responseBody);

    return responseBody;
}

bool MatrixClient::readHTTPResponse(String &body, String &headers) {
    int ch_count = 0;
    unsigned long now = millis();
    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    bool responseReceived = false;

    while (millis() - now < syncTimeout + waitForResponse) {
        while (client->available()) {
            responseReceived = true;
            char c = client->read();
            if (!finishedHeaders) {
                if (currentLineIsBlank && c == '\n') {
                    finishedHeaders = true;
                } else {
                    headers += c;
                }
            } else {
                if (ch_count < maxMessageLength) {
                    body += c;
                    ch_count++;
                }
            }

            if (c == '\n') currentLineIsBlank = true;
            else if (c != '\r') currentLineIsBlank = false;
        }

        if (responseReceived) {
            body = extractJsonBody(body);
            break;
        }
    }
    return responseReceived;
}


String MatrixClient::extractJsonBody(const String& responseBody) {
    int startIndex = responseBody.indexOf('{');
    int endIndex = responseBody.lastIndexOf('}');
    if (startIndex != -1 && endIndex != -1) {
        return responseBody.substring(startIndex, endIndex + 1);
    }
    return responseBody;
}

bool MatrixClient::extractNextBatch(const String& responseBody) {
    int index = responseBody.indexOf("\"next_batch\":\"");
    if (index != -1) {
        int start = index + 14; // Length of "\"next_batch\":\""
        int end = responseBody.indexOf("\"", start);
        if (end != -1) {
            syncToken = responseBody.substring(start, end);
            return true;
        }
    }
    return false;
}

void MatrixClient::storeEvent(const MatrixEvent& event) {
    recentEvents.push_back(event);
}

std::vector<MatrixEvent> MatrixClient::getRecentEvents() {
    std::vector<MatrixEvent> events = recentEvents;
    recentEvents.clear();
    return events;
}

void MatrixClient::logger(const String& message) {
    unsigned long currentMillis = millis();
    unsigned long hours = (currentMillis / 3600000) % 24;
    unsigned long minutes = (currentMillis / 60000) % 60;
    unsigned long seconds = (currentMillis / 1000) % 60;
    unsigned long milliseconds = currentMillis % 1000;

    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds, milliseconds);

    Serial.print("[");
    Serial.print(timeBuffer);
    Serial.print("] ");
    Serial.println(message);
}