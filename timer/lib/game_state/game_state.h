#pragma once

#include <Arduino.h>
#include <can_bus.h>

enum GameState
{
    GAME_IDLE,
    GAME_RUNNING,
    GAME_EXPLODED,
    GAME_SOLVED
};

class GameStateManager
{
private:
    GameState currentState = GAME_IDLE;
    unsigned long stateChangeTime = 0;

    // Strike and module state
    uint8_t strikeCount = 0;
    uint8_t maxStrikes = 3;
    uint8_t totalModules = 0;
    uint8_t solvedModules = 0;

    // Serial
    String serial = "";

    // Timer state
    unsigned long timeLimitMillis = 300000; // default 5 mins
    unsigned long remainingMillis = 300000;
    unsigned long lastUpdate = 0;
    bool timerRunning = false;

public:
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

    // Strike management
    void setStrikes(uint8_t strikes) { strikeCount = (strikes > maxStrikes) ? maxStrikes : strikes; }
    uint8_t getStrikes() const { return strikeCount; }
    void incrementStrikes()
    {
        if (strikeCount < maxStrikes)
            strikeCount++;
    }
    void setMaxStrikes(uint8_t max) { maxStrikes = max; }
    uint8_t getMaxStrikes() const { return maxStrikes; }

    // Module tracking
    void setTotalModules(uint8_t total) { totalModules = total; }
    uint8_t getTotalModules() const { return totalModules; }
    void setSolvedModules(uint8_t solved) { solvedModules = solved; }
    void incrementSolvedModules()
    {
        if (solvedModules < totalModules)
            solvedModules++;
    }
    uint8_t getSolvedModules() const { return solvedModules; }
    bool allModulesSolved() const { return solvedModules >= totalModules && totalModules > 0; }

    // Serial management
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
        const char letters[] = "ABCDEFGHJKLMNPQRSTUVWXZ";
        const char digits[] = "0123456789";
        char serialBuf[7];
        bool hasLetter = false;
        bool hasDigit = false;

        for (int i = 0; i < 5; ++i)
        {
            if (random(2) == 0)
            {
                serialBuf[i] = letters[random(sizeof(letters) - 1)];
                hasLetter = true;
            }
            else
            {
                serialBuf[i] = digits[random(10)];
                hasDigit = true;
            }
        }

        if (!hasLetter)
            serialBuf[random(5)] = letters[random(sizeof(letters) - 1)];
        if (!hasDigit)
            serialBuf[random(5)] = digits[random(10)];
        serialBuf[5] = digits[random(10)];
        serialBuf[6] = '\0';

        serial = String(serialBuf);
    }

    // Time configuration
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
        }
        else
        {
            remainingMillis -= adjusted;
        }
    }

    unsigned long getRemainingMillis() const { return remainingMillis; }
    bool isTimerRunning() const { return timerRunning; }
};
