# Timer web API

Implemented in `timer/lib/web_server/`. `web_html.cpp` is the page. `web_api.cpp` is the JSON handlers. `web_utils.cpp` is shared formatting. `web_server.cpp` accepts the TCP connection and switches on the path.

Add a route in `web_api.cpp` and call it from the request switch in `web_server.cpp`. Stay on ArduinoJson 6. Call `updateWebServer()` from `loop` on core 0.

## Access

| Mode | How you reach it |
|---|---|
| No credentials stored | AP `KTANE_TIMER_SETUP`, password `ktane12345`, `http://<softAP IP>/` |
| Credentials stored | The timer joins that network. `GET /api/wifi` reports the mode. |

`POST /api/wifi` body:

```json
{ "ssid": "shop-network", "password": "secret" }
```

Empty SSID is rejected. The password may be empty. The response says whether the join succeeded. A failed join still keeps the saved credentials for the next boot.

`GET /api/wifi` returns the stored SSID and does not return the password.

## Routes

| Method | Path | Body | Effect |
|---|---|---|---|
| `GET` | `/` | | HTML setup and game page |
| `GET` | `/wifi` | | WiFi page |
| `GET` | `/api/all` | | State, config, gameplay `modules`, plus `status.audioModules` and `status.serialModules`. The HTML page polls this. |
| `GET` | `/api/status` | | State, time remaining, strikes, serial, edgework counts, volume. No module arrays. |
| `GET` | `/api/config` | | Strike and alarm settings |
| `GET` | `/api/modules` | | Registered gameplay modules. Audio and the serial tag are omitted here. |
| `GET` | `/api/audio` | | `volume`, `connected`, and `lastSeenMs` for `CAN_ID_AUDIO` |
| `GET` | `/api/wifi` | | Current WiFi mode and SSID |
| `POST` | `/api/ping` | | Liveness |
| `POST` | `/api/command` | `{ "command": "..." }` | See below |
| `POST` | `/api/config` | JSON fields | Replaces `GameConfig` |
| `POST` | `/api/audio` | `{ "volume": 0..100 }` | `setAudioVolume`, which sends `AUDIO_SET_VOLUME` |
| `POST` | `/api/wifi` | `{ "ssid", "password" }` | Save and try to join |

While the AP is up, a `GET` that is not `/`, `/wifi`, or under `/api/` is redirected to the AP address.

## Commands

`POST /api/command` with `Content-Type: application/json`.

| `command` | When it works |
|---|---|
| `start` | `IDLE` calls `startGame`. `PAUSED` calls `resumeTimer`. Any other state returns 400, "Cannot start from current state". |
| `pause` | `RUNNING` only. Otherwise "Game is not running". |
| `reset` | Any state. `reset()`. |
| `addStrike` | `addStrike()` |
| `clearStrikes` | `clearStrikes()` |
| `solveModule` | Marks the first unsolved module whose id is in `0x10`..`0x6F` solved. This walks the timer's module table, not a CAN id range from `can_bus.h`. |
| `setTime:M:SS` | The suffix is parsed at the colon, for example `setTime:5:00`. Sets the time limit and calls `resetTimer()`. A missing colon returns "Invalid time format". |

Unknown commands return 400, "Unknown command". A missing `command` field returns "Invalid request".

```powershell
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d "{\"command\":\"start\"}"
```

## Config body

`GET /api/config` returns `success` and a `config` object:

| Field | Default |
|---|---|
| `maxStrikes` | 3 |
| `enableStrikeAcceleration` | true |
| `strikeAccelerationFactor` | 0.25 |
| `enableEmergencyAlarm` | true |
| `emergencyAlarmThreshold` | 60000 |
| `enableNeedyModules` | true |
| `enableEdgework` | true |

`POST /api/config` accepts those fields at the top of the JSON document (not nested under `config`). Missing fields fall back to the defaults above. `timeLimitMs` is forced back to 300000 inside `handleSetConfig`. Change the clock with `setTime:M:SS`, not with this route.

`POST /api/audio` clamps `volume` to 0..100. Non-numeric values are rejected. The timer stores the percent and sends `AUDIO_SET_VOLUME` to `CAN_ID_AUDIO`. The audio module scales mixed PCM. It does not rewrite the codec PGA.
