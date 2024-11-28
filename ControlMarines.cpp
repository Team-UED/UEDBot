#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control SCVs
void BasicSc2Bot::ControlMarines() {
	TargetMarines();
}

void BasicSc2Bot::TargetMarines() {
    const ObservationInterface* observation = Observation();
    Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    
    if (marines.empty()) {
        return;
    }

    // For each marine
    for (const auto& marine : marines) {
        // Skip marines already attacking or using abilities
        if (!marine->orders.empty() && 
            (marine->orders.front().ability_id == ABILITY_ID::ATTACK || 
             marine->orders.front().ability_id == ABILITY_ID::EFFECT_STIM)) {
            continue;
        }

        // Find closest enemy unit
        const Unit* target = nullptr;
        float min_distance = std::numeric_limits<float>::max();

        for (const auto& enemy : observation->GetUnits(Unit::Alliance::Enemy)) {
            // Skip invalid targets
            if (!enemy->is_alive || IsWorkerUnit(enemy) || IsTrivialUnit(enemy)) {
                continue;
            }

            float distance = Distance2D(marine->pos, enemy->pos);
            if (distance < min_distance) {
                min_distance = distance;
                target = enemy;
            }
        }

        // If target found, attack it
        if (target) {
            // Use stimpacks if close enough to enemy
            if (min_distance <= 7.5f) {
                UseStimPack(marine);
            }
            Actions()->UnitCommand(marine, ABILITY_ID::ATTACK, target);
        }
        // If no target found and not in combat, move to rally point or attack enemy base
        else if (!is_attacking) {
            Point2D rally = GetRallyPoint();
            if (Distance2D(marine->pos, rally) > 5.0f) {
                Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, rally);
            }
        }
        else {
            Actions()->UnitCommand(marine, ABILITY_ID::ATTACK, enemy_start_location);
        }
    }
}

void BasicSc2Bot::UseStimPack(const Unit* marine) {
    // Skip invalid or dead Marines
    if (!marine || !marine->is_alive) {
        return;
    }

    // Skip marines that are already using Stim Pack
    if (!marine->orders.empty()) {
        for (const auto& order : marine->orders) {
            if (order.ability_id == ABILITY_ID::EFFECT_STIM) {
                return;
            }
        }
    }

    // Check if Marine's health is sufficient to use Stim Pack (costs 10 HP)
    if (marine->health > 20.0f) {
        // Check if Marine is near enemies
        for (const auto& enemy : Observation()->GetUnits(Unit::Alliance::Enemy)) {

            // Skip invalid targets
            if (!enemy || !enemy->is_alive || IsWorkerUnit(enemy)|| IsTrivialUnit(enemy)) {
                continue; 
            }

            // Only use Stim Pack if enemies are nearby 
            if (Distance2D(marine->pos, enemy->pos) <= 7.5f) {
                Actions()->UnitCommand(marine, ABILITY_ID::EFFECT_STIM);
                return; 
            }
        }
    }
}

