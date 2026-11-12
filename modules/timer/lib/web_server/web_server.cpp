#include "web_server.h"
#include "web_html.h"
#include "web_api.h"
#include "web_utils.h"
#include <ktane_console.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <string.h>

static WiFiServer* server = nullptr;
static WiFiUDP* dnsServer = nullptr;
static bool wifiConfigured = false;
static bool inAPMode = false;

// AP mode credentials (used when no WiFi credentials are stored)
const char* ap_ssid = "KTANE_TIMER_SETUP";
const char* ap_password = "ktane12345";

// Maximum lengths for SSID and password
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64

// EEPROM storage structure
struct WiFiCredentials {
    char magic[4];        // "WIFI" magic bytes to verify data is valid
    char ssid[MAX_SSID_LEN + 1];
    char password[MAX_PASSWORD_LEN + 1];
};

#define EEPROM_SIZE 512
#define WIFI_CRED_MAGIC "WIFI"
#define WIFI_CRED_ADDR 0

bool loadWiFiCredentials(String& ssid, String& password) {
    EEPROM.begin(EEPROM_SIZE);
    
    WiFiCredentials creds;
    EEPROM.get(WIFI_CRED_ADDR, creds);
    
    // Check magic bytes to verify data is valid
    bool isValid = (creds.magic[0] == 'W' && 
                    creds.magic[1] == 'I' && 
                    creds.magic[2] == 'F' && 
                    creds.magic[3] == 'I' &&
                    creds.ssid[0] != '\0');
    
    EEPROM.end();
    
    if (isValid) {
        ssid = String(creds.ssid);
        password = String(creds.password);
        return true;
    }
    
    return false;
}

bool saveWiFiCredentials(const String& ssid, const String& password) {
    if (ssid.length() == 0 || ssid.length() > MAX_SSID_LEN) {
        return false;
    }
    if (password.length() > MAX_PASSWORD_LEN) {
        return false;
    }
    
    EEPROM.begin(EEPROM_SIZE);
    
    WiFiCredentials creds;
    // Set magic bytes
    creds.magic[0] = 'W';
    creds.magic[1] = 'I';
    creds.magic[2] = 'F';
    creds.magic[3] = 'I';
    
    // Copy SSID and password
    memset(creds.ssid, 0, sizeof(creds.ssid));
    memset(creds.password, 0, sizeof(creds.password));
    strncpy(creds.ssid, ssid.c_str(), MAX_SSID_LEN);
    strncpy(creds.password, password.c_str(), MAX_PASSWORD_LEN);
    
    EEPROM.put(WIFI_CRED_ADDR, creds);
    EEPROM.commit();
    EEPROM.end();
    
    return true;
}

bool clearWiFiCredentials() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Clear magic bytes to invalidate credentials
    WiFiCredentials creds;
    memset(&creds, 0, sizeof(creds));
    EEPROM.put(WIFI_CRED_ADDR, creds);
    EEPROM.commit();
    EEPROM.end();
    
    return true;
}

bool connectToWiFi(const String& ssid, const String& password) {
    KTANE_CONSOLE_OUT.print("Connecting to WiFi: ");
    KTANE_CONSOLE_OUT.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Wait for connection with timeout
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        KTANE_CONSOLE_OUT.print(".");
        attempts++;
    }
    KTANE_CONSOLE_OUT.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        KTANE_CONSOLE_OUT.print("WiFi connected! IP address: ");
        KTANE_CONSOLE_OUT.println(WiFi.localIP());
        wifiConfigured = true;
        inAPMode = false;
        return true;
    } else {
        KTANE_CONSOLE_OUT.println("WiFi connection failed!");
        wifiConfigured = false;
        return false;
    }
}

// DNS packet structure (network byte order)
struct DNSHeader {
    uint8_t id[2];
    uint8_t flags[2];
    uint8_t qdcount[2];
    uint8_t ancount[2];
    uint8_t nscount[2];
    uint8_t arcount[2];
};

// Handle DNS requests for captive portal
void handleDNS() {
    if (!dnsServer || !inAPMode) return;
    
    int packetSize = dnsServer->parsePacket();
    if (packetSize > 0 && packetSize < 512) {
        IPAddress apIP = WiFi.softAPIP();
        uint8_t buffer[512];
        int bytesRead = dnsServer->read(buffer, packetSize);
        
        if (bytesRead < sizeof(DNSHeader)) return;
        
        // Simple DNS response - respond with AP IP for all queries
        DNSHeader* header = (DNSHeader*)buffer;
        
        // Check if this is a query (QR bit not set)
        uint16_t flags = (header->flags[0] << 8) | header->flags[1];
        if ((flags & 0x8000) == 0) {
            // Set response flags (in network byte order)
            header->flags[0] = 0x81;
            header->flags[1] = 0x80; // Response, standard query response, no error
            header->ancount[0] = header->qdcount[0];
            header->ancount[1] = header->qdcount[1];
            header->nscount[0] = 0;
            header->nscount[1] = 0;
            header->arcount[0] = 0;
            header->arcount[1] = 0;
            
            // Find the question section and create answer
            uint8_t* qname = buffer + sizeof(DNSHeader);
            uint16_t qtype = 0;
            uint16_t qclass = 0;
            
            // Skip QNAME (null-terminated labels)
            uint8_t* ptr = qname;
            int nameLen = 0;
            while (*ptr != 0 && (ptr - buffer) < bytesRead && nameLen < 255) {
                if ((*ptr & 0xC0) == 0xC0) {
                    // Compressed name pointer
                    ptr += 2;
                    break;
                }
                uint8_t labelLen = *ptr;
                ptr += labelLen + 1;
                nameLen += labelLen + 1;
            }
            if (*ptr == 0) ptr++; // Skip null terminator
            
            // Check bounds
            if ((ptr - buffer + 4) > bytesRead) return;
            
            // Read QTYPE and QCLASS
            qtype = (ptr[0] << 8) | ptr[1];
            qclass = (ptr[2] << 8) | ptr[3];
            ptr += 4;
            
            // Respond to all A record queries (type 1) and ANY queries (type 255)
            // Also respond to AAAA (type 28) queries for IPv6 compatibility
            if ((qtype == 1 || qtype == 255 || qtype == 28) && qclass == 1) {
                // For AAAA queries, return empty response (no IPv6 support)
                if (qtype == 28) {
                    // Send empty response for AAAA queries
                    IPAddress remoteIP = dnsServer->remoteIP();
                    uint16_t remotePort = dnsServer->remotePort();
                    dnsServer->beginPacket(remoteIP, remotePort);
                    dnsServer->write(buffer, sizeof(DNSHeader) + (ptr - qname));
                    dnsServer->endPacket();
                    return;
                }
                // Create answer section
                // Name pointer (compressed - points to question at offset 12)
                ptr[0] = 0xC0;
                ptr[1] = 0x0C; // Points to start of question (offset 12)
                ptr += 2;
                
                // Type A (1) - always return A record even if query was ANY
                ptr[0] = 0x00;
                ptr[1] = 0x01;
                ptr += 2;
                
                // Class IN (1)
                ptr[0] = 0x00;
                ptr[1] = 0x01;
                ptr += 2;
                
                // TTL (60 seconds)
                ptr[0] = 0x00;
                ptr[1] = 0x00;
                ptr[2] = 0x00;
                ptr[3] = 0x3C;
                ptr += 4;
                
                // Data length (4 bytes for IPv4)
                ptr[0] = 0x00;
                ptr[1] = 0x04;
                ptr += 2;
                
                // IP address
                ptr[0] = apIP[0];
                ptr[1] = apIP[1];
                ptr[2] = apIP[2];
                ptr[3] = apIP[3];
                ptr += 4;
                
                // Send response
                IPAddress remoteIP = dnsServer->remoteIP();
                uint16_t remotePort = dnsServer->remotePort();
                dnsServer->beginPacket(remoteIP, remotePort);
                dnsServer->write(buffer, ptr - buffer);
                dnsServer->endPacket();
                
                KTANE_CONSOLE_OUT.print("DNS: Responded to query (type=");
                KTANE_CONSOLE_OUT.print(qtype);
                KTANE_CONSOLE_OUT.print(") with IP ");
                KTANE_CONSOLE_OUT.println(apIP);
            }
        }
    }
}

void startAPMode() {
    KTANE_CONSOLE_OUT.print("Starting WiFi AP: ");
    KTANE_CONSOLE_OUT.println(ap_ssid);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    delay(100);
    
    IPAddress IP = WiFi.softAPIP();
    KTANE_CONSOLE_OUT.print("AP IP address: ");
    KTANE_CONSOLE_OUT.println(IP);
    inAPMode = true;
    wifiConfigured = false;
    
    // Start DNS server for captive portal
    // Note: The AP's DHCP server should automatically set DNS to this IP
    if (!dnsServer) {
        dnsServer = new WiFiUDP();
    }
    if (!dnsServer->begin(53)) {
        KTANE_CONSOLE_OUT.println("Warning: Failed to start DNS server on port 53");
    } else {
        KTANE_CONSOLE_OUT.println("DNS server started on port 53 for captive portal");
        KTANE_CONSOLE_OUT.print("AP IP: ");
        KTANE_CONSOLE_OUT.println(IP);
        KTANE_CONSOLE_OUT.println("Connect to WiFi and captive portal should open automatically");
    }
}

void initWebServer(GameStateManager* gsm) {
    // Set the game state pointer for API handlers
    gameStatePtr = gsm;

    // Try to load stored WiFi credentials
    String ssid, password;
    if (loadWiFiCredentials(ssid, password)) {
        KTANE_CONSOLE_OUT.println("Found stored WiFi credentials, attempting to connect...");
        if (connectToWiFi(ssid, password)) {
            KTANE_CONSOLE_OUT.println("Successfully connected to WiFi!");
        } else {
            KTANE_CONSOLE_OUT.println("Failed to connect to WiFi, starting AP mode for configuration");
            startAPMode();
        }
    } else {
        KTANE_CONSOLE_OUT.println("No WiFi credentials found, starting AP mode for configuration");
        startAPMode();
    }

    // Create web server
    server = new WiFiServer(80);
    server->begin();
    KTANE_CONSOLE_OUT.println("Web server started on port 80");
}

// Check if path is a captive portal detection endpoint
bool isCaptivePortalPath(const String& path, String& responseContent) {
    // Common captive portal detection endpoints
    String lowerPath = path;
    lowerPath.toLowerCase();
    
    // Windows 11 - expects "Microsoft Connect Test" text
    if (lowerPath == "/connecttest.txt" || lowerPath.indexOf("msftconnecttest.com") >= 0) {
        responseContent = "Microsoft Connect Test";
        return true;
    }
    
    // iOS - expects specific HTML
    if (lowerPath == "/hotspot-detect.html" || lowerPath.indexOf("captive.apple.com") >= 0) {
        responseContent = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
        return true;
    }
    
    // Android/Chrome - expects 204 No Content
    if (lowerPath == "/generate_204" || 
        lowerPath == "/connectivitycheck.gstatic.com/generate_204" ||
        lowerPath == "/gstatic.com/generate_204" ||
        lowerPath == "/gen_204" ||
        lowerPath.indexOf("connectivitycheck") >= 0) {
        responseContent = ""; // Empty for 204
        return true;
    }
    
    // Firefox
    if (lowerPath.indexOf("detectportal.firefox.com") >= 0) {
        responseContent = "success";
        return true;
    }
    
    // Other common endpoints
    if (lowerPath == "/canonical.html" ||
        lowerPath == "/success.txt" ||
        lowerPath == "/ncsi.txt" ||
        lowerPath == "/library/test/success.html" ||
        lowerPath == "/kindle-wifi/wifiredirect.html") {
        responseContent = "success";
        return true;
    }
    
    return false;
}

void updateWebServer() {
    if (!server) return;
    
    // Handle DNS requests for captive portal
    if (inAPMode && dnsServer) {
        handleDNS();
    }
    
    WiFiClient client = server->available();
    
    if (client && client.connected() && client.available()) {
        String method, path;
        if (parseRequest(client, method, path)) {
            // Captive portal: redirect all requests to /wifi when in AP mode
            // (except API calls and the wifi page itself)
            if (inAPMode && !path.startsWith("/api/") && path != "/wifi" && path != "/") {
                // Check if it's a captive portal detection endpoint
                String portalResponse;
                if (isCaptivePortalPath(path, portalResponse)) {
                    // Android/Chrome expects 204 No Content
                    if (path.indexOf("generate_204") >= 0 || path.indexOf("connectivitycheck") >= 0) {
                        String response = "HTTP/1.1 204 No Content\r\n";
                        response += "Connection: close\r\n";
                        response += "Content-Length: 0\r\n\r\n";
                        client.print(response);
                    } else {
                        // Windows, iOS, Firefox expect specific content
                        // iOS needs HTML content type
                        bool isHTML = portalResponse.indexOf("<HTML>") >= 0;
                        String response = "HTTP/1.1 200 OK\r\n";
                        response += "Content-Type: ";
                        response += isHTML ? "text/html" : "text/plain";
                        response += "\r\n";
                        response += "Connection: close\r\n";
                        response += "Content-Length: ";
                        response += String(portalResponse.length());
                        response += "\r\n";
                        // Add cache control headers to prevent caching
                        response += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
                        response += "Pragma: no-cache\r\n";
                        response += "Expires: 0\r\n";
                        response += "\r\n";
                        response += portalResponse;
                        client.print(response);
                    }
                } else {
                    // Redirect to WiFi configuration page with proper headers
                    String redirect = "HTTP/1.1 302 Found\r\n";
                    redirect += "Location: http://";
                    redirect += WiFi.softAPIP().toString();
                    redirect += "/wifi\r\n";
                    redirect += "Connection: close\r\n";
                    redirect += "Content-Length: 0\r\n\r\n";
                    client.print(redirect);
                }
            } else if (method == "GET" && path == "/") {
                // In AP mode, redirect root to wifi config, otherwise show main page
                if (inAPMode) {
                    // For captive portal, serve a simple redirect page that auto-redirects
                    String redirectPage = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"0;url=/wifi\"><title>Redirecting...</title></head><body><p>Redirecting to <a href=\"/wifi\">WiFi Configuration</a>...</p><script>window.location.href='/wifi';</script></body></html>";
                    sendResponse(client, 200, "text/html", redirectPage);
                } else {
                    // Serve main HTML page
                    sendResponse(client, 200, "text/html", String(html_page));
                }
            } else if (method == "GET" && path == "/api/all") {
                handleAll(client);
            } else if (method == "GET" && path == "/api/status") {
                handleStatus(client);
            } else if (method == "GET" && path == "/api/config") {
                handleGetConfig(client);
            } else if (method == "GET" && path == "/api/modules") {
                handleModules(client);
            } else if (method == "GET" && path == "/api/audio") {
                handleGetAudio(client);
            } else if (method == "POST" && path == "/api/ping") {
                handlePing(client);
            } else if (method == "POST" && path == "/api/command") {
                // Read POST body with timeout
                String body = "";
                unsigned long bodyStartTime = millis();
                while (client.available() && (millis() - bodyStartTime < 500)) {
                    body += (char)client.read();
                }
                handleCommand(client, body);
            } else if (method == "POST" && path == "/api/config") {
                // Read POST body with timeout
                String body = "";
                unsigned long bodyStartTime = millis();
                while (client.available() && (millis() - bodyStartTime < 500)) {
                    body += (char)client.read();
                }
                handleSetConfig(client, body);
            } else if (method == "POST" && path == "/api/audio") {
                String body = "";
                unsigned long bodyStartTime = millis();
                while (client.available() && (millis() - bodyStartTime < 500)) {
                    body += (char)client.read();
                }
                handleSetAudio(client, body);
            } else if (method == "GET" && path == "/wifi") {
                // Serve WiFi configuration page
                sendResponse(client, 200, "text/html", String(wifi_config_page));
            } else if (method == "GET" && path == "/api/wifi") {
                handleGetWiFi(client);
            } else if (method == "POST" && path == "/api/wifi") {
                // Read POST body with timeout
                String body = "";
                unsigned long bodyStartTime = millis();
                while (client.available() && (millis() - bodyStartTime < 500)) {
                    body += (char)client.read();
                }
                handleSetWiFi(client, body);
            } else {
                sendResponse(client, 404, "text/plain", "Not found");
            }
        }
        
        // Close connection after response
        client.flush();
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

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String getWiFiMode() {
    if (WiFi.getMode() == WIFI_AP) {
        return "AP";
    } else if (WiFi.getMode() == WIFI_STA) {
        return "STA";
    } else if (WiFi.getMode() == WIFI_AP_STA) {
        return "AP_STA";
    } else {
        return "OFF";
    }
}

