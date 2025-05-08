#pragma once

#include <Arduino.h>

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

    // Game data
    uint8_t strikeCount = 0;
    uint8_t maxStrikes = 3;
    uint8_t totalModules = 0;
    uint8_t solvedModules = 0;

public:
    void setState(GameState newState)
    {
        if (currentState != newState)
        {
            currentState = newState;
            stateChangeTime = millis();
        }
    }

    GameState getState() const
    {
        return currentState;
    }

    unsigned long timeSinceStateChange() const
    {
        return millis() - stateChangeTime;
    }

    bool is(GameState state) const
    {
        return currentState == state;
    }

    // Strike management
    void setStrikes(uint8_t strikes)
    {
        strikeCount = strikes;
    }

    uint8_t getStrikes() const
    {
        return strikeCount;
    }

    void incrementStrikes()
    {
        if (strikeCount < maxStrikes)
        {
            strikeCount++;
        }
    }

    void setMaxStrikes(uint8_t max)
    {
        maxStrikes = max;
    }

    uint8_t getMaxStrikes() const
    {
        return maxStrikes;
    }

    // Module tracking
    void setTotalModules(uint8_t total)
    {
        totalModules = total;
    }

    uint8_t getTotalModules() const
    {
        return totalModules;
    }

    void setSolvedModules(uint8_t solved)
    {
        solvedModules = solved;
    }

    void incrementSolvedModules()
    {
        if (solvedModules < totalModules)
        {
            solvedModules++;
        }
    }

    uint8_t getSolvedModules() const
    {
        return solvedModules;
    }

    bool allModulesSolved() const
    {
        return solvedModules >= totalModules && totalModules > 0;
    }
};
