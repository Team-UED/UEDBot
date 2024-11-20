#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control SCVs
void BasicSc2Bot::ControlMarines() {
	TargetMarines();
}

void BasicSc2Bot::TargetMarines() {

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

