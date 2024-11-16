#include "BasicSc2Bot.h"

void BasicSc2Bot::Offense() {
    if (num_marines > 30 && num_siege_tanks > 1) {
        is_attacking = true;
    }
    if (is_attacking) {
        AllOutRush();
    }
}

void BasicSc2Bot::AllOutRush() {
    // Get all our siege tanks and marines
    Units marines = Observation()->GetUnits(Unit::Alliance::Self,
                                            IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    Units siege_tanks = Observation()->GetUnits(
        Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

    // Move all our units to the enemy base
    for (const auto &marine : marines) {
        Actions()->UnitCommand(marine, ABILITY_ID::ATTACK,
                               enemy_start_location);
    }
    for (const auto &tank : siege_tanks) {
        Actions()->UnitCommand(tank, ABILITY_ID::ATTACK, enemy_start_location);
    }

    // Ensure we have marines to use as reference for enemy proximity
    if (marines.empty()) {
        return;
    }

    // Get all enemy units near the first marine
    Units enemy_units = Observation()->GetUnits(
        Unit::Alliance::Enemy, [marines](const Unit &unit) {
            return Distance2D(unit.pos, marines.front()->pos) < 20.0f &&
                   unit.display_type == Unit::DisplayType::Visible;
        });

    if (enemy_units.empty()) {
        return; // No enemies nearby, continue moving to enemy base
    }

    // Find the highest threat enemy unit
    const Unit *primary_target = nullptr;
    for (const auto &enemy : enemy_units) {
        if (!primary_target ||
            enemy->weapon_cooldown < primary_target->weapon_cooldown) {
            primary_target = enemy;
        }
    }

    if (primary_target) {
        // Attack the primary target
        for (const auto &marine : marines) {
            Actions()->UnitCommand(marine, ABILITY_ID::ATTACK,
                                   primary_target->pos);
        }
        for (const auto &tank : siege_tanks) {
            // Move tanks toward the primary target 
            if (Distance2D(tank->pos, primary_target->pos) > 10.0f) {
                Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE,
                                       primary_target->pos);
            }
        }
    }
}
