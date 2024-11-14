
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
    // Radius around ally base for defense
    const float ally_base_defense_radius = 30.0f;

    // Check if any enemy is near the ally base
    bool enemy_near_ally_base = false;
    for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
        float distance_to_base = Distance2D(start_location, enemy_unit->pos);
        if (distance_to_base < ally_base_defense_radius) {
            enemy_near_ally_base = true;
            break;
        }
    }

    // Disable Jump if enemy is near ally base
    if (enemy_near_ally_base) {
        return;
    }

    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        // Check if the unit is a Battlecruiser with full health
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && unit->health >= unit->health_max) {
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



// Target enemy units based on threat levels
void BasicSc2Bot::Target() {
    
	// Detect radius for Battlecruisers
    const float defense_check_radius = 12.0f;

    // Distance threshold
    const float ally_base_defense_radius = 30.0f;
    
	// Dynamically set threat levels for enemy units
    const Units battlecruisers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

	// Exit when there are no battlecruisers
	if (battlecruisers.empty()) {
		return;
	}

    bool enemy_near_ally_base = false;

    for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
        float distance_to_base = Distance2D(start_location, enemy_unit->pos);
        if (distance_to_base < ally_base_defense_radius) {
            enemy_near_ally_base = true;
            break;
        }
    }

    if (!enemy_near_ally_base) {
        return;
    }


	// Threshold for "kiting" behavior
    int num = static_cast<int>(battlecruisers.size());
    const int threat_threshold = 10 * num ;  

    // Get map dimensions for corner coordinates
    const GameInfo& game_info = Observation()->GetGameInfo();
    const Point2D playable_min = game_info.playable_min;
    const Point2D playable_max = game_info.playable_max;

    // Define the four corners of the map
    std::vector<Point2D> map_corners = {
        Point2D(playable_min.x, playable_min.y),  // Bottom-left
        Point2D(playable_max.x, playable_min.y),  // Bottom-right
        Point2D(playable_min.x, playable_max.y),  // Top-left
        Point2D(playable_max.x, playable_max.y)   // Top-right
    };


    for (const auto& battlecruiser : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
        int total_threat = 0;
        const Unit* target = nullptr;
        constexpr float max_distance = std::numeric_limits<float>::max();
        float min_distance = max_distance;
        float min_hp = std::numeric_limits<float>::max();

        const Unit* closest_enemy = nullptr;
        float closest_enemy_distance = max_distance;

        // Calculate total threat level and find the closest enemy
        for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
            auto threat_it = threat_levels.find(enemy_unit->unit_type);
            if (threat_it != threat_levels.end()) {
                float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                if (distance < defense_check_radius) {
                    total_threat += threat_it->second;

                    if (distance < closest_enemy_distance) {
                        closest_enemy_distance = distance;
                        closest_enemy = enemy_unit;
                    }
                }
            }
        }

        // Determine whether to kite based on the threat level
        bool should_kite = total_threat >= threat_threshold;

        if (should_kite) {
            // Existing kiting logic
            // Find the nearest corner (vertex)
            Point2D nearest_corner;
            float min_corner_distance = max_distance;

            for (const auto& corner : map_corners) {
                float corner_distance = DistanceSquared2D(battlecruiser->pos, corner);
                if (corner_distance < min_corner_distance) {
                    min_corner_distance = corner_distance;
                    nearest_corner = corner;
                }
            }

            // Issue SMART command to the nearest corner
            Actions()->UnitCommand(battlecruiser, ABILITY_ID::SMART, nearest_corner);
        }
        else {
            // Existing targeting logic
            // Prioritize high-threat enemy units
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                auto threat_it = threat_levels.find(enemy_unit->unit_type);
                if (threat_it != threat_levels.end()) {
                    float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                    if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                        min_distance = distance;
                        min_hp = enemy_unit->health;
                        target = enemy_unit;
                    }
                }
            }

            // If no high-threat targets are found, prioritize workers
            if (!target) {
                std::vector<UNIT_TYPEID> worker_types = {
                    UNIT_TYPEID::TERRAN_SCV,
                    UNIT_TYPEID::PROTOSS_PROBE,
                    UNIT_TYPEID::ZERG_DRONE,
                    UNIT_TYPEID::ZERG_QUEEN
                };

                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(worker_types.begin(), worker_types.end(), enemy_unit->unit_type) != worker_types.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // Then target any units
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    bool is_structure = false;
                    for (const auto& attribute : unit_type_data.attributes) {
                        if (attribute == Attribute::Structure) {
                            is_structure = true;
                            break;
                        }
                    }
                    if (!is_structure) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // Then target any supply units
            if (!target) {
                std::vector<UNIT_TYPEID> resource_units = {
                    UNIT_TYPEID::ZERG_OVERLORD,
                    UNIT_TYPEID::TERRAN_SUPPLYDEPOT,
                    UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED,
                    UNIT_TYPEID::PROTOSS_PYLON
                };

                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(resource_units.begin(), resource_units.end(), enemy_unit->unit_type) != resource_units.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // Then target any structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    bool is_structure = false;
                    for (const auto& attribute : unit_type_data.attributes) {
                        if (attribute == Attribute::Structure) {
                            is_structure = true;
                            break;
                        }
                    }
                    if (is_structure) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // Attack the selected target
            if (target) {
                Actions()->UnitCommand(battlecruiser, ABILITY_ID::ATTACK_ATTACK, target);
            }
        }
    }
}


void BasicSc2Bot::Retreat() {
    
}

