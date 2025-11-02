# Web Server Module

A web-based control interface for the KTANE Timer module using WiFi AP mode on the Raspberry Pi Pico W.

## Features

- **WiFi Access Point**: Creates its own WiFi network for easy access
- **Modern UI**: Built with Tailwind CSS for a beautiful, responsive interface
- **Real-time Status**: Auto-refreshes every 500ms to show current game state
- **Full Game Controls**: All debug interface controls available via web
- **Configuration Panel**: Adjust game settings without code changes
- **Module Status**: View connected modules and their status
- **Edgework Display**: See serial number, batteries, indicators, and ports

## WiFi Access

- **SSID**: `KTANE_TIMER`
- **Password**: `ktane12345`
- **IP**: `192.168.4.1` (default AP IP)

After connecting to the WiFi network, open a browser and navigate to `http://192.168.4.1`

## Web Interface Controls

### Game Controls
- **Start**: Begin a new game (or resume from paused)
- **Pause**: Pause the running game
- **Reset**: Reset game to initial state
- **Add Strike**: Add a strike to the bomb
- **Clear Strikes**: Reset strike count to zero
- **Solve Module**: Automatically solve the first unsolved module
- **Set Time**: Change the countdown time limit
- **Config**: Open configuration panel

### Configuration Options
- **Max Strikes**: Maximum number of strikes before explosion
- **Strike Acceleration**: Enable/disable timer acceleration with strikes
- **Acceleration Factor**: Rate of time acceleration per strike
- **Emergency Alarm**: Enable emergency time warnings
- **Emergency Threshold**: Time remaining for emergency state (ms)
- **Needy Modules**: Enable needy module timers
- **Edgework**: Enable edgework generation

## API Endpoints

### GET `/api/status`
Returns current game state and status information.

**Response**:
```json
{
  "success": true,
  "state": "RUNNING",
  "timeRemaining": 280000,
  "strikes": 1,
  "maxStrikes": 3,
  "totalModules": 5,
  "solvedModules": 2,
  "remainingModules": 3,
  "serialNumber": "AB3C9D",
  "batteries": 2,
  "indicators": 3,
  "ports": 2
}
```

### POST `/api/command`
Execute a game control command.

**Request**:
```json
{
  "command": "start"
}
```

**Commands**: `start`, `pause`, `reset`, `addStrike`, `clearStrikes`, `solveModule`, `setTime:mm:ss`

### GET `/api/config`
Get current game configuration.

### POST `/api/config`
Update game configuration.

**Request**:
```json
{
  "maxStrikes": 5,
  "enableStrikeAcceleration": true,
  "strikeAccelerationFactor": 0.25,
  "enableEmergencyAlarm": true,
  "emergencyAlarmThreshold": 60000,
  "enableNeedyModules": true,
  "enableEdgework": true
}
```

## Implementation

The web server uses:
- **Arduino-Pico WiFi Library**: Native WiFi support for Raspberry Pi Pico W ([Documentation](https://arduino-pico.readthedocs.io/en/latest/wifi.html))
- **WiFiServer**: Built-in synchronous HTTP server for Pico W
- **ArduinoJson**: JSON parsing and generation
- **WiFi AP Mode**: Creates its own WiFi network

The server uses synchronous handling but processes requests quickly to avoid blocking the game loop. WiFi operations are efficient thanks to LWIP running in the background.

## Customization

To change WiFi credentials, edit the constants in `web_server.cpp`:
```cpp
const char* ssid = "KTANE_TIMER";
const char* password = "ktane12345";
```

To modify the UI, edit the `html_page` string in `web_server.cpp`.

