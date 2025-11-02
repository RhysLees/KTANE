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
    String request = client.readStringUntil('\r');
    client.read(); // skip \n
    
    int firstSpace = request.indexOf(' ');
    int secondSpace = request.indexOf(' ', firstSpace + 1);
    
    if (firstSpace == -1 || secondSpace == -1) {
        return false;
    }
    
    method = request.substring(0, firstSpace);
    path = request.substring(firstSpace + 1, secondSpace);
    
    // Read rest of headers
    while (client.available()) {
        String line = client.readStringUntil('\r');
        client.read(); // skip \n
        if (line.length() == 0) break;
    }
    
    return true;
}

