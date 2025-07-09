#include "module_tracker.h"
#include <can_bus.h>

static ModuleTracker *trackerInstance = nullptr;

ModuleTracker::ModuleTracker(GameStateManager *gsm)
    : gameState(gsm) {}

void ModuleTracker::handleCanMessage(uint16_t id, uint8_t *data, uint8_t len)
{
    if (id >= MODULE_ID_START && id <= MODULE_ID_END)
    {
        modules[id].lastSeen = millis();
        
        // Update the game state that we've seen this module
        if (gameState) {
            gameState->updateModuleSeen(id);
        }
    }
}

void ModuleTracker::update()
{
    unsigned long now = millis();
    bool changed = false;

    for (auto it = modules.begin(); it != modules.end();)
    {
        if (now - it->second.lastSeen > timeoutMs)
        {
            // Module timed out, remove it
            uint16_t moduleId = it->first;
            it = modules.erase(it);
            changed = true;
            
            // Unregister from game state
            if (gameState) {
                gameState->unregisterModule(moduleId);
            }
        }
        else
        {
            ++it;
        }
    }
}

void ModuleTracker::reset()
{
    modules.clear();
    // Game state manager handles its own module tracking
}

static void moduleTrackerCanCallback(uint16_t id, const uint8_t *data, uint8_t len)
{
    if (trackerInstance)
    {
        trackerInstance->handleCanMessage(id, const_cast<uint8_t *>(data), len);
    }
}

void initModuleTracker(GameStateManager *gsm)
{
    static ModuleTracker tracker(gsm);
    trackerInstance = &tracker;
    registerCanCallback(moduleTrackerCanCallback);
}
