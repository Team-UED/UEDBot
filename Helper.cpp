#include "BasicSc2Bot.h"

const Unit* BasicSc2Bot::GetMainBase() const {
    // Get the main Command Center
    Units command_centers = Observation()->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
    if (!command_centers.empty()) {
        return command_centers.front();
    }
    return nullptr;
}