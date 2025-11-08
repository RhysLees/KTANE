#include <Arduino.h>
#include <can_bus.h>
#include <module_state.h>
#include <simon_says.h>

namespace {

constexpr int STATUS_LED_PIN = 11;

SimonSays simonSays;
ModuleStatus currentStatus = MODULE_STATUS_IDLE;
uint8_t lastProgress = 0xFF;
bool lastSolved = false;

void updateModuleStatus(ModuleStatus status) {
    if (status == currentStatus) {
        return;
    }
    setModuleStateStatus(status);
    currentStatus = status;
}

void onGameStateChanged(bool running) {
    simonSays.setGameRunning(running);
    if (!running) {
        updateModuleStatus(MODULE_STATUS_IDLE);
        setModuleStateSolved(false);
        setModuleStateProgress(0);
    } else {
        updateModuleStatus(MODULE_STATUS_ACTIVE);
    }
}

void onStrikeCountUpdated(uint8_t strikes) {
    simonSays.setStrikeCount(strikes);
}

void onSerialNumberUpdated(const String& serial) {
    simonSays.setSerialNumber(serial);
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    moduleStateHandleCanMessage(id, senderId, data, len);
}

ModuleStatus phaseToModuleStatus(SimonSays::Phase phase) {
    switch (phase) {
        case SimonSays::Phase::Solved:
            return MODULE_STATUS_SOLVED;
        case SimonSays::Phase::Idle:
            return MODULE_STATUS_IDLE;
        default:
            return MODULE_STATUS_ACTIVE;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);

    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    assignUniqueId(CAN_TYPE_SIMON);

    registerCanCallback(onCanMessage);

    initModuleState(STATUS_LED_PIN);
    updateModuleStatus(MODULE_STATUS_IDLE);
    setModuleStateProgress(0);
    setModuleStateSolved(false);

    if (globalModuleState) {
        globalModuleState->setGameStateCallback(onGameStateChanged);
        globalModuleState->setStrikeCallback(onStrikeCountUpdated);
        globalModuleState->setSerialNumberCallback(onSerialNumberUpdated);

        simonSays.setGameRunning(globalModuleState->isGameRunning());
        simonSays.setStrikeCount(globalModuleState->getStrikeCount());
        simonSays.setSerialNumber(globalModuleState->getSerialNumber());
    }

    simonSays.begin();
}

void loop() {
    handleCanMessages();
    updateModuleState();

    simonSays.update();

    if (simonSays.consumeStrikeTriggered() && globalModuleState) {
        globalModuleState->triggerStrike();
    }

    ModuleStatus desiredStatus = phaseToModuleStatus(simonSays.phase());
    updateModuleStatus(desiredStatus);

    uint8_t progress = simonSays.progressPercent();
    if (progress != lastProgress) {
        setModuleStateProgress(progress);
        lastProgress = progress;
    }

    bool solved = simonSays.isSolved();
    if (solved != lastSolved) {
        setModuleStateSolved(solved);
        lastSolved = solved;
    }
}
