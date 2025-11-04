#include "web_utils.h"

// Helper function to send HTTP response
void sendResponse(WiFiClient& client, int code, const String& contentType, const String& body) {
    client.print("HTTP/1.1 ");
    client.print(code);
    client.println(" OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.print("Content-Length: ");
    client.println(body.length());
    client.println("Connection: close");
    client.println();
    client.print(body);
}

// Helper function to format game state
String formatGameState(GameState state) {
    switch (state) {
        case GameState::IDLE: return "IDLE";
        case GameState::RUNNING: return "RUNNING";
        case GameState::PAUSED: return "PAUSED";
        case GameState::EXPLODED: return "EXPLODED";
        case GameState::DEFUSED: return "DEFUSED";
        case GameState::VICTORY: return "VICTORY";
        default: return "UNKNOWN";
    }
}

// Parse HTTP request
bool parseRequest(WiFiClient& client, String& method, String& path) {
    String request = "";
    unsigned long startTime = millis();
    
    // Read with timeout to avoid blocking
    // Reduced timeout and add connection check
    while (client.available() == 0) {
        if (millis() - startTime > 50) {  // Reduced from 100ms to 50ms
            return false; // Timeout
        }
        if (!client.connected()) {
            return false; // Client disconnected
        }
        yield();
    }
    
    // Read request line
    while (client.available()) {
        char c = client.read();
        if (c == '\r') {
            client.read(); // skip \n
            break;
        }
        request += c;
    }
    
    int firstSpace = request.indexOf(' ');
    int secondSpace = request.indexOf(' ', firstSpace + 1);
    
    if (firstSpace == -1 || secondSpace == -1) {
        return false;
    }
    
    method = request.substring(0, firstSpace);
    path = request.substring(firstSpace + 1, secondSpace);
    
    // Read rest of headers with timeout
    startTime = millis();
    while (client.available()) {
        if (millis() - startTime > 100) {  // Reduced from 200ms to 100ms
            break; // Timeout for headers
        }
        if (!client.connected()) {
            break; // Client disconnected
        }
        String line = "";
        unsigned long lineStartTime = millis();
        while (client.available()) {
            if (millis() - lineStartTime > 50) {  // Timeout per line
                break;
            }
            char c = client.read();
            if (c == '\r') {
                if (client.available()) {
                    client.read(); // skip \n
                }
                break;
            }
            line += c;
        }
        if (line.length() == 0) break;
    }
    
    return true;
}

