#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Siege Tanks
void BasicSc2Bot::ControlSiegeTanks() {
    SiegeMode();
	TargetSiegeTank();
}

void BasicSc2Bot::SiegeMode() {
    const float enemy_detection_radius = 15.0f;

    // Get all Siege Tanks that belong to the bot
    const Units siege_tanks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
    const Units siege_tanks_sieged = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANKSIEGED));

    if (siege_tanks.empty() && siege_tanks_sieged.empty()) {
        return;
    }


    for (const auto& tank : siege_tanks) {
        bool enemy_nearby = false;
        // Check for nearby enemies within the detection radius
        for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
            float distance_to_enemy = Distance2D(tank->pos, enemy_unit->pos);

            if (distance_to_enemy <= enemy_detection_radius) {
                enemy_nearby = true;
                break;
            }
        }
        // Determine if the tank should be in Siege Mode
        if (enemy_nearby) {
            Actions()->UnitCommand(tank, ABILITY_ID::MORPH_SIEGEMODE);
        }
    }

    for (const auto& tank : siege_tanks_sieged) {
        bool enemy_nearby = false;
        // Check for nearby enemies within the detection radius
        for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
            float distance_to_enemy = Distance2D(tank->pos, enemy_unit->pos);

            if (distance_to_enemy <= enemy_detection_radius) {
                enemy_nearby = true;
                break;
            }
        }
        // Determine if the tank should be in Siege Mode
        if (!enemy_nearby) {
            Actions()->UnitCommand(tank, ABILITY_ID::MORPH_UNSIEGE);
        }
    }
}

void BasicSc2Bot::TargetSiegeTank() {
}