#include "web_server.h"
#include "web_html.h"
#include "web_api.h"
#include "web_utils.h"
#include <WiFi.h>

static WiFiServer* server = nullptr;

// WiFi credentials (change these or use AP mode)
const char* ssid = "KTANE_GAME";
const char* password = "ktane12345";

void initWebServer(GameStateManager* gsm) {
    // Set the game state pointer for API handlers
    gameStatePtr = gsm;

    // Start WiFi AP
    Serial.print("Starting WiFi AP: ");
    Serial.println(ssid);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    delay(100);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Create web server
    server = new WiFiServer(80);
    server->begin();
    Serial.println("Web server started on port 80");
}

void updateWebServer() {
    if (!server) return;
    
    WiFiClient client = server->available();
    
    if (client && client.available()) {
        String method, path;
        if (parseRequest(client, method, path)) {
            // Handle different routes
            if (method == "GET" && path == "/") {
                // Serve main HTML page
                sendResponse(client, 200, "text/html", String(html_page));
            } else if (method == "GET" && path == "/api/status") {
                handleStatus(client);
            } else if (method == "GET" && path == "/api/config") {
                handleGetConfig(client);
            } else if (method == "GET" && path == "/api/debug") {
                handleCanLog(client);
            } else if (method == "GET" && path == "/api/modules") {
                handleModules(client);
            } else if (method == "POST" && path == "/api/command") {
                // Read POST body
                String body = "";
                while (client.available()) {
                    body += (char)client.read();
                }
                handleCommand(client, body);
            } else if (method == "POST" && path == "/api/config") {
                // Read POST body
                String body = "";
                while (client.available()) {
                    body += (char)client.read();
                }
                handleSetConfig(client, body);
            } else {
                sendResponse(client, 404, "text/plain", "Not found");
            }
        }
        
        // Close connection after response
        delay(1);
        client.stop();
    }
}

String getWiFiIP() {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

