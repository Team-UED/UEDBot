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

    // Marine parameters
    const float marine_range = 13.0f;             // Marine's vision
    const float fallback_distance = 1.0f;        // Distance to kite away for melee units
    const float advance_distance = 0.5f;         // Distance to close for ranged units

    // For each Marine
    for (const auto& marine : marines) {
        const Unit* target = nullptr;
        float min_distance = std::numeric_limits<float>::max(); 

        // Find the closest enemy 
        for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
            // Skip invalid or dead units
            if (!enemy_unit || !enemy_unit->is_alive) {
                continue;
            }

            // Calculate distance to the enemy unit
            float distance_to_enemy = Distance2D(marine->pos, enemy_unit->pos);

            // Find the closest unit
            if (distance_to_enemy < min_distance && distance_to_enemy < 13.0f) {
                min_distance = distance_to_enemy;
                target = enemy_unit; // Update the target to the closest unit
            }
        }

        if (target) {
            // Check if the target is a melee unit
            bool is_melee = melee_units.find(target->unit_type) != melee_units.end();
            float distance_to_target = Distance2D(marine->pos, target->pos);

            if (marine->weapon_cooldown == 0.0f) {
                Actions()->UnitCommand(marine, ABILITY_ID::ATTACK_ATTACK, target);
			}
            else {
                if (is_melee && Distance2D(marine->pos,target->pos) <= 4.5f) {
                    // Fall back if the target is melee
                    Point2D fallback_direction = marine->pos - target->pos;
                    float length = std::sqrt(fallback_direction.x * fallback_direction.x + fallback_direction.y * fallback_direction.y);
                    if (length > 0) {
                        fallback_direction /= length;
                    }
                    Point2D fallback_position = marine->pos + fallback_direction * fallback_distance;
                    Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, fallback_position);
                }
                else {
                    // Check if the target is ranged but not a structure
                    const UnitTypeData& target_type_data = Observation()->GetUnitTypeData().at(target->unit_type);
                    bool is_structure = false;

                    for (const auto& attribute : target_type_data.attributes) {
                        if (attribute == Attribute::Structure) {
                            is_structure = true;
                            break;
                        }
                    }

                    // Advance if the target is ranged and not a structure
                    if (!is_structure && Distance2D(marine->pos, target->pos) > 4.5f) {
                        Point2D advance_direction = target->pos - marine->pos;
                        float length = std::sqrt(advance_direction.x * advance_direction.x + advance_direction.y * advance_direction.y);
                        if (length > 0) {
                            advance_direction /= length;
                        }
                        Point2D advance_position = marine->pos + advance_direction * advance_distance;
                        Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, advance_position);
                    }
                }
            }
        }
    }
}
