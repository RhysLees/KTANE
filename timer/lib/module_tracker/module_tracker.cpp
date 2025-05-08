#include <module_tracker.h>
#include <can_bus.h>

static ModuleTracker *trackerInstance = nullptr;

ModuleTracker::ModuleTracker(GameStateManager *gsm)
    : gameState(gsm) {}

void ModuleTracker::handleCanMessage(uint8_t id, uint8_t *data, uint8_t len)
{
    if (id >= MODULE_ID_START && id <= MODULE_ID_END)
    {
        modules[id].lastSeen = millis();
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
            it = modules.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }

    if (gameState && changed)
    {
        gameState->setTotalModules(modules.size());
    }
}

void ModuleTracker::reset()
{
    modules.clear();
    if (gameState)
    {
        gameState->setTotalModules(0);
    }
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
