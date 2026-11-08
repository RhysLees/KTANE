---
name: arduinojson
description: >-
  Build and parse JSON with ArduinoJson 6 on the timer web API. Use when
  editing timer/lib/web_server, DynamicJsonDocument, deserializeJson, or
  serializeJson. Stay on version 6; do not switch the timer to ArduinoJson 7.
---

# ArduinoJson 6

`bblanchon/ArduinoJson@^6.21.0` in `timer/platformio.ini`. Include `<ArduinoJson.h>`.

The timer's HTTP handlers in `timer/lib/web_server/web_api.cpp` construct a `DynamicJsonDocument`, fill it, and `serializeJson` into the response. Capacities already in that file: 200, 256, 300, 512, 1024, 4096, 8192.

Version 7 replaces `DynamicJsonDocument` and `StaticJsonDocument` with a single growable `JsonDocument`. This firmware is pinned to 6. Keep the capacity constructor. A v7-style `JsonDocument doc;` will not compile against 6.21, and relaxing the pin pulls a larger library onto a 264 KB RP2040.

Docs: <https://arduinojson.org/v6/api/>

## Documents

```cpp
StaticJsonDocument<256> small;   // stack, fine under about 1 KB
DynamicJsonDocument doc(1024);   // heap, fixed capacity, does not grow
```

`capacity` is the memory pool in bytes, not the length of the JSON text. A document that returns `NoMemory` needs a larger pool. Size with the [ArduinoJson assistant](https://arduinojson.org/v6/assistant/) or `JSON_OBJECT_SIZE(n)` + `JSON_ARRAY_SIZE(n)` + string bytes.

The allocator is monotonic: `remove()` does not return bytes to the pool. Build a document, serialize it, and let it go out of scope. Do not keep one global document and mutate it for the life of the process.

`doc.overflowed()` is true after a failed allocation. `doc.memoryUsage()` and `doc.capacity()` explain a near-miss.

## Parse

```cpp
DynamicJsonDocument doc(512);
DeserializationError err = deserializeJson(doc, client);
if (err) {
  // err.c_str(): EmptyInput, IncompleteInput, InvalidInput,
  // NoMemory, TooDeep, NotSupported
  return;
}
int strikes = doc["strikes"] | 0;          // default when missing or wrong type
const char* serial = doc["serial"] | "";
```

`deserializeJson` clears the document first. Overloads take `const char*`, `char*` (zero-copy, buffer must outlive the document), `String`, `Stream&` (`WiFiClient` is a stream), and flash strings.

`deserializeJson(doc, input, DeserializationOption::NestingLimit(10))` raises the default depth of 10. The timer payloads are flat.

Also in the header, unused here: `deserializeMsgPack`.

## Build

```cpp
DynamicJsonDocument doc(1024);
doc["ok"] = true;
doc["state"] = "RUNNING";
doc["strikes"] = 1;

JsonArray modules = doc.createNestedArray("modules");
JsonObject module = modules.createNestedObject();
module["type"] = "SIMON";
module["solved"] = false;

doc["label"] = serialized("\"already-json\"");  // insert raw JSON

String body;
serializeJson(doc, body);          // ArduinoJson 6 appends to String
serializeJson(doc, client);        // or a Print / Stream
serializeJsonPretty(doc, client);  // larger, fine for the debug UI
```

In version 6, `serializeJson` into a `String` appends. Clear the string first when reusing it. (`measureJson(doc)` returns the byte count if you need to reserve.)

Nested objects in 6.21 use `createNestedObject("key")` and `createNestedArray("key")`. The v7 pattern `doc["key"].to<JsonObject>()` is a different major version.

## Read back

```cpp
if (doc.containsKey("timeLimitMs")) {
  unsigned long ms = doc["timeLimitMs"].as<unsigned long>();
}
for (JsonObject m : doc["modules"].as<JsonArray>()) {
  const char* type = m["type"];
}
```

Missing keys compare equal to 0, false, and nullptr. Use `containsKey` or the `|` default when a missing field must be distinguished from zero. `is<int>()`, `is<const char*>()`, `isNull()` check types.

Copy `const char*` results out of the document before it is destroyed. They point into the pool (or into the zero-copy input buffer).

## What the timer should keep doing

- One `DynamicJsonDocument` per request, on the stack frame of the handler, capacity matching the reply.
- Check `DeserializationError` before reading a posted body.
- Leave the library constraint at `^6.21.0`.
