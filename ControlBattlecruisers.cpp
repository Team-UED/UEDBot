
#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Battlecruisers
void BasicSc2Bot::ControlBattlecruisers() {
    Jump();
	Target();
	Retreat();
}

// Use Tactical Jump into enemy base
void BasicSc2Bot::Jump() {

    // Distance threshold
    const float threshold = 100.0f; 

    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        // Check if the unit is a Battlecruiser with full health
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && unit->health >= unit->health_max) {

            // Calculate the distance from the enemy base,
            // proceed if the Battlecruiser is far enough from the enemy base
            float distance = sc2::Distance2D(unit->pos, enemy_start_location);
            if (distance > threshold) {

                // Check if Tactical Jump ability is available
                auto abilities = Query()->GetAbilitiesForUnit(unit);
                bool tactical_jump_available = false;

                for (const auto& ability : abilities.abilities) {
                    if (ability.ability_id == ABILITY_ID::EFFECT_TACTICALJUMP) {
                        tactical_jump_available = true;
                        break;
                    }
                }

                // Use Tactical Jump if available
                if (tactical_jump_available) {
                    Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_TACTICALJUMP, enemy_start_location);
                }
            }
        }
    }
}

// Target units attacking Battlecruisers first if air defense is weak,
// otherwise prioritize workers.
void BasicSc2Bot::Target() {
    const float air_defense_threshold = 5;  
    const float defense_check_radius = 10.0f;

    for (const auto& battlecruiser : Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
        int air_defense_count = 0;
        const sc2::Unit* target = nullptr;
        constexpr float max_distance = std::numeric_limits<float>::max();
        float min_distance = max_distance;

        // Count nearby enemy anti-air units to assess air defense strength
        for (const auto& enemy_unit : Observation()->GetUnits(sc2::Unit::Alliance::Enemy)) {
            if (anti_air_units.count(enemy_unit->unit_type) > 0) {  // Check if the unit type is in the anti-air set
                if (sc2::Distance2D(battlecruiser->pos, enemy_unit->pos) < defense_check_radius) {
                    air_defense_count++;
                }
            }
        }

        // If air defense is weak, prioritize attacking units
        if (air_defense_count < air_defense_threshold) {
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                // Check if the enemy unit has an attack order
                for (const auto& order : enemy_unit->orders) {
                    if (order.ability_id == ABILITY_ID::ATTACK_ATTACK || order.ability_id == ABILITY_ID::ATTACK) {
                        if (order.target_unit_tag == battlecruiser->tag) {
                            float distance = sc2::Distance2D(battlecruiser->pos, enemy_unit->pos);
                            if (distance < min_distance) {
                                min_distance = distance;
                                target = enemy_unit;
                            }
                        }
                    }
                }
            }
        }

        if (!target) {
            // If no attacking units were found or if air defense is strong, target workers
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                if (enemy_unit->unit_type == UNIT_TYPEID::ZERG_QUEEN ||
                    enemy_unit->unit_type == UNIT_TYPEID::TERRAN_SCV ||
                    enemy_unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE ||
                    enemy_unit->unit_type == UNIT_TYPEID::ZERG_DRONE
                    ) {

                    float distance = sc2::Distance2D(battlecruiser->pos, enemy_unit->pos);
                    if (distance < min_distance) {
                        min_distance = distance;
                        target = enemy_unit;
                    }
                }
            }
        }

        // Attack the target
        if (target) {
            Actions()->UnitCommand(battlecruiser, ABILITY_ID::ATTACK_ATTACK, target);
        }
    }
}

void BasicSc2Bot::Retreat() {
    const float air_defense_threshold = 5;  
    const float defense_check_radius = 10.0f;  
    const float retreat_health_threshold = 150.0f;  

    for (const auto& battlecruiser : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
        int air_defense_count = 0;
        const sc2::Unit* closest_enemy = nullptr;
        constexpr float max_distance = std::numeric_limits<float>::max();
        float min_distance = max_distance;

        // Count nearby anti-air units to assess air defense strength
        for (const auto& enemy_unit : Observation()->GetUnits(sc2::Unit::Alliance::Enemy)) {
            if (anti_air_units.count(enemy_unit->unit_type) > 0) {  
                if (sc2::Distance2D(battlecruiser->pos, enemy_unit->pos) < defense_check_radius) {
                    air_defense_count++;
                }
            }
        }

        // Check if Battlecruiser's HP is below the retreat threshold
        if (battlecruiser->health < retreat_health_threshold) {
            // Retreat to base
            Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, start_location);
            continue;  
        }

        if (air_defense_count >= air_defense_threshold && closest_enemy) {
            sc2::Point2D escape_direction = battlecruiser->pos + (battlecruiser->pos - closest_enemy->pos);
            Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, escape_direction);
        }
    }
}

