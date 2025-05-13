#pragma once

#include <Arduino.h>
#include <can_bus.h>
#include <vector>
#include <algorithm>

// --- Game States ---
enum GameState
{
    GAME_IDLE,
    GAME_RUNNING,
    GAME_EXPLODED,
    GAME_SOLVED
};

// --- Module Structs ---
struct ModuleState
{
    uint16_t canId;
    uint8_t type;
    bool isSolved = false;
};

struct NeedyModuleState
{
    uint16_t canId;
    unsigned long intervalMs = 10000;
    unsigned long nextActivationTime = 0;
    bool active = false;
};

// --- Edgework Struct ---
struct Edgework
{
    std::vector<String> indicators;
    std::vector<String> ports;
    uint8_t batteryCount = 0;

    bool hasIndicator(const String &label) const
    {
        return std::find(indicators.begin(), indicators.end(), label) != indicators.end();
    }

    bool hasPort(const String &label) const
    {
        return std::find(ports.begin(), ports.end(), label) != ports.end();
    }
};

// --- Main Class ---
class GameStateManager
{
private:
    GameState currentState = GAME_IDLE;
    unsigned long stateChangeTime = 0;

    uint8_t strikeCount = 0;
    uint8_t maxStrikes = 3;

    String serial = "";

    unsigned long timeLimitMillis = 300000;
    unsigned long remainingMillis = 300000;
    unsigned long lastUpdate = 0;
    bool timerRunning = false;

    std::vector<ModuleState> modules;
    std::vector<NeedyModuleState> needyModules;
    Edgework edgework;

public:
    // --- Core Loop Hook ---
    void tick()
    {
        updateRemaining();
        updateNeedyModules();
    }

    // --- State Management ---
    void setState(GameState newState)
    {
        if (currentState != newState)
        {
            currentState = newState;
            stateChangeTime = millis();
        }
    }

    GameState getState() const { return currentState; }
    bool is(GameState state) const { return currentState == state; }
    unsigned long timeSinceStateChange() const { return millis() - stateChangeTime; }

    // --- Strikes ---
    void setStrikes(uint8_t strikes) { strikeCount = min(strikes, maxStrikes); }
    uint8_t getStrikes() const { return strikeCount; }
    void incrementStrikes()
    {
        if (strikeCount < maxStrikes)
            strikeCount++;
    }
    void setMaxStrikes(uint8_t max) { maxStrikes = max; }
    uint8_t getMaxStrikes() const { return maxStrikes; }

    // --- Serial Number ---
    void setSerial(const String &s)
    {
        serial = s.substring(0, 6);
        uint8_t buf[7];
        buf[0] = SERIAL_DISPLAY_SET_SERIAL;
        memcpy(&buf[1], serial.c_str(), 6);
        sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 7);
    }

    String getSerial() const { return serial; }

    void generateSerial()
    {
        const char letters[] = "ABCDEFGHJKLMNPQRSTUVWXZ"; // A-Z excluding O and Y
        const char alphanum[] = "ABCDEFGHJKLMNPQRSTUVWXZ0123456789";
        const char digits[] = "0123456789";

        char serialBuf[7];
        serialBuf[0] = alphanum[random(sizeof(alphanum) - 1)];
        serialBuf[1] = alphanum[random(sizeof(alphanum) - 1)];
        serialBuf[2] = digits[random(10)];
        serialBuf[3] = letters[random(sizeof(letters) - 1)];
        serialBuf[4] = letters[random(sizeof(letters) - 1)];
        serialBuf[5] = alphanum[random(sizeof(alphanum) - 1)];
        serialBuf[6] = '\0';

        serial = String(serialBuf);
    }

    // --- Timer ---
    void setTimeLimit(unsigned long ms)
    {
        timeLimitMillis = ms;
        remainingMillis = ms;
    }

    void resetTimer()
    {
        remainingMillis = timeLimitMillis;
        lastUpdate = millis();
        timerRunning = false;
    }

    void startTimer()
    {
        lastUpdate = millis();
        timerRunning = true;
    }

    void stopTimer()
    {
        if (timerRunning)
        {
            updateRemaining();
            timerRunning = false;
        }
    }

    void updateRemaining()
    {
        if (!timerRunning)
            return;
        unsigned long now = millis();
        unsigned long delta = now - lastUpdate;
        lastUpdate = now;

        float speed = 1.0f + 0.25f * strikeCount;
        float adjusted = delta * speed;

        if (adjusted >= remainingMillis)
        {
            remainingMillis = 0;
            timerRunning = false;
            currentState = GAME_EXPLODED;
            sendCanMessage(CAN_ID_AUDIO, (uint8_t[]){AUDIO_EXPLODED}, 1);
        }
        else
        {
            remainingMillis -= adjusted;
        }
    }

    unsigned long getRemainingMillis() const { return remainingMillis; }
    bool isTimerRunning() const { return timerRunning; }

    // --- Module Management ---
    void registerModule(uint16_t canId, uint8_t type)
    {
        modules.push_back({canId, type, false});
    }

    void setModuleSolved(uint16_t canId)
    {
        for (auto &mod : modules)
        {
            if (mod.canId == canId)
            {
                mod.isSolved = true;
                break;
            }
        }
    }

    bool isModuleSolved(uint16_t canId) const
    {
        for (const auto &mod : modules)
        {
            if (mod.canId == canId)
                return mod.isSolved;
        }
        return false;
    }

    uint8_t getTotalModules() const { return modules.size(); }
    uint8_t getSolvedModules() const
    {
        uint8_t count = 0;
        for (const auto &mod : modules)
        {
            if (mod.isSolved)
                count++;
        }
        return count;
    }

    bool allModulesSolved() const
    {
        return !modules.empty() && getSolvedModules() == modules.size();
    }

    // --- Needy Module Management ---
    void registerNeedyModule(uint16_t canId, unsigned long intervalMs)
    {
        needyModules.push_back({canId, intervalMs, millis() + intervalMs, false});
    }

    void updateNeedyModules()
    {
        unsigned long now = millis();
        for (auto &mod : needyModules)
        {
            if (!mod.active && now >= mod.nextActivationTime)
            {
                mod.active = true;
                mod.nextActivationTime = now + mod.intervalMs;
                uint8_t msg[] = {0x01}; // NEEDY_TRIGGER (example)
                sendCanMessage(mod.canId, msg, 1);
            }
        }
    }

    // --- Edgework ---
    void setupEdgework()
    {
        const char *ind[] = {"FRK", "CAR", "NSA", "TRN", "MSA", "SND", "BOB"};
        const char *port[] = {"PARALLEL", "SERIAL", "PS/2", "RJ-45", "RCA", "DVI-D"};

        edgework.indicators.clear();
        for (int i = 0; i < 3; i++)
        {
            if (random(2))
                edgework.indicators.push_back(ind[random(7)]);
        }

        edgework.ports.clear();
        for (int i = 0; i < 2; i++)
        {
            if (random(2))
                edgework.ports.push_back(port[random(6)]);
        }

        edgework.batteryCount = random(1, 5);
    }

    const ModuleState &getModule(uint8_t index) const
    {
        return modules.at(index);
    }

    const Edgework &getEdgework() const { return edgework; }
    bool hasIndicator(const String &label) const { return edgework.hasIndicator(label); }
    bool hasPort(const String &label) const { return edgework.hasPort(label); }
    uint8_t getBatteryCount() const { return edgework.batteryCount; }
};
