#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control SCVs
void BasicSc2Bot::ControlMarines() {
	TargetMarines();
}

void BasicSc2Bot::TargetMarines() {

    // Get all Marines
    Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));

    if (marines.empty()) {
        return;
    }

    // For each marine
    for (const auto& marine : marines) {
        if (!marine->is_alive || marine->orders.empty()) {
            continue;
        }

        const Unit* target = nullptr;
        float min_distance = std::numeric_limits<float>::max();
        float min_hp = std::numeric_limits<float>::max();

        auto UpdateTarget = [&](const Unit* enemy_unit, float max_distance) {
            float distance = Distance2D(marine->pos, enemy_unit->pos);
            if (!enemy_unit->is_alive || distance > max_distance) {
                return;
            }
            if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                min_distance = distance;
                min_hp = enemy_unit->health;
                target = enemy_unit;
            }
        };
        Actions()->UnitCommand(marine, ABILITY_ID::ATTACK_ATTACK, target);
    }
}
