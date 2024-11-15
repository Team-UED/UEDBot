
#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Battlecruisers
void BasicSc2Bot::ControlBattlecruisers() {
	Jump();
	Target();
    RetreatCheck();
}

// Use Tactical Jump into enemy base
void BasicSc2Bot::Jump() {

	if (IsMainBaseUnderAttack()) {
		return;
	}

    const float enemy_base_radius = 50.0f;

    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        // Check if the unit is a Battlecruiser with full health and not retreating
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER &&
            unit->health >= unit->health_max) {

            // Skip if the Battlecruiser is currently retreating
            if (battlecruiser_retreating[unit]) {
                continue;
            }

            float distance = sc2::Distance2D(unit->pos, enemy_start_location);

			// Check if the Battlecruiser is within the enemy base
            if (distance > enemy_base_radius) {
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



/// Target enemy units based on threat levels
void BasicSc2Bot::Target() {

    // Detect radius for Battlecruisers
    const float defense_check_radius = 12.0f;

    // Distance from enemy
    const float max_distance_from_enemy = 15.0f;

    // Dynamically set threat levels for enemy units
    const Units battlecruisers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

    // Exit when there are no battlecruisers
    if (battlecruisers.empty()) {
        return;
    }

    // Number of Battlecruisers in combat
    int num_battlecruisers_in_combat = 0;

    for (const auto& battlecruiser : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
        bool is_near_enemy = false;
        for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
            float distance_to_enemy = Distance2D(battlecruiser->pos, enemy_unit->pos);
            if (distance_to_enemy <= max_distance_from_enemy) {
                is_near_enemy = true;
                break;
            }
        }

        // Only count Battlecruisers near at least one enemy unit
        if (is_near_enemy) {
            num_battlecruisers_in_combat++;
        }
    }



    // Threshold for "kiting" behavior
    const int threat_threshold = 10 * num_battlecruisers_in_combat;


    for (const auto& battlecruiser : battlecruisers) {
        // Disables targetting while Jumping
        if (!battlecruiser->orders.empty()) {
            const AbilityID current_ability = battlecruiser->orders.front().ability_id;
            if (current_ability == ABILITY_ID::EFFECT_TACTICALJUMP) {
                continue;
            }
        }
        // Disables targetting while Retreating
        if (battlecruiser_retreating[battlecruiser]) {
            continue;
        }

        int total_threat = 0;
        const Unit* target = nullptr;
        constexpr float max_distance = std::numeric_limits<float>::max();
        float min_distance = max_distance;
        float min_hp = std::numeric_limits<float>::max();

        // Calculate total threat level
        for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
            auto threat = threat_levels.find(enemy_unit->unit_type);

            if (threat != threat_levels.end()) {
                float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);

                if (distance < defense_check_radius) {
                    total_threat += threat->second;

                }
            }
        }

        // Determine whether to retreat based on the threat level
        // retreat if the total threat level is above the threshold
        if (total_threat >= threat_threshold) {

            float health_threshold = 0.0f;

            // Extremly Strong anti air units -> Retreat when hp = 400
            if (total_threat >= 2.0 * threat_threshold) {
                health_threshold = 400.0f;
            }
            // Strong anti air units -> Retreat when hp = 400
            else if (total_threat >= 1.5 * threat_threshold && total_threat <= 2.0 * threat_threshold) {
                health_threshold = 300.0f;
            }
            // Moderate anti air units -> Retreat when hp = 200
            else if (total_threat > threat_threshold && total_threat <= 1.5 * threat_threshold) {
                health_threshold = 200.0f;
            }

            if (battlecruiser->health <= health_threshold || (battlecruiser->health <= 100.0f && total_threat > 2)) {
                Retreat(battlecruiser);
            }
            else {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    // Ensure the enemy unit is alive
                    if (!enemy_unit->is_alive) {
                        continue;
                    }
                    auto threat = threat_levels.find(enemy_unit->unit_type);

                    if (threat != threat_levels.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }

                if (target) {

                    // Maximum distance to consider for kiting
                    const float max_kite_distance = 12.0f;

                    // Calculate the distance to the target
                    float distance_to_target = Distance2D(battlecruiser->pos, target->pos);

                    // Skip kiting if the target is too far away
                    if (distance_to_target > max_kite_distance) {
                        continue;
                    }


                    Point2D nearest_corner;
                    float min_corner_distance = std::numeric_limits<float>::max();;

                    // Find nearest corner to the enemy start location
                    for (const auto& corner : map_corners) {
                        float corner_distance = DistanceSquared2D(battlecruiser->pos, corner);
                        if (corner_distance < min_corner_distance) {
                            min_corner_distance = corner_distance;
                            nearest_corner = corner;
                        }
                    }

                    // Calculate the direction away from the target 
                    Point2D kite_direction = battlecruiser->pos - target->pos;


                    // Normalize the kite direction vector
                    float kite_length = std::sqrt(kite_direction.x * kite_direction.x + kite_direction.y * kite_direction.y);
                    if (kite_length > 0) {
                        kite_direction /= kite_length;
                    }

                    // Define the kiting distance (range of Battlecruiser)
                    float kiting_distance = 6.0f;


                    // Calculate the avoidance direction from the enemy vertex
                    Point2D avoidance_direction = battlecruiser->pos - nearest_corner;
                    float avoidance_length = std::sqrt(avoidance_direction.x * avoidance_direction.x + avoidance_direction.y * avoidance_direction.y);
                    if (avoidance_length > 0) {
                        avoidance_direction /= avoidance_length;
                    }

                    // Define weights for combining directions
                    // Weight for kiting direction
                    float kite_weight = 0.1f;
                    // Weight for avoiding enemy vertex
                    float avoidance_weight = 0.9f;

                    // Combine directions to get the final direction
                    Point2D final_direction = (kite_direction * kite_weight) + (avoidance_direction * avoidance_weight);

                    // Normalize the final direction
                    float final_length = std::sqrt(final_direction.x * final_direction.x + final_direction.y * final_direction.y);
                    if (final_length > 0) {
                        final_direction /= final_length;
                    }

                    // Calculate the final kite position
                    Point2D kite_position = battlecruiser->pos + final_direction * kiting_distance;

                    // Move the Battlecruiser to the calculated kite position
                    Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, kite_position);
                }
            }

        }
        // Do not kite if the total threat level is below the threshold
        else {

            // 1st Priority -> Attacking enemy units
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                auto threat = threat_levels.find(enemy_unit->unit_type);
                if (threat != threat_levels.end()) {
                    if (enemy_unit->unit_type != UNIT_TYPEID::ZERG_SPORECRAWLER &&
                        enemy_unit->unit_type != UNIT_TYPEID::TERRAN_MISSILETURRET &&
                        enemy_unit->unit_type != UNIT_TYPEID::PROTOSS_PHOTONCANNON) {
                    }
                    float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                    if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                        if (!enemy_unit->is_alive) {
                            continue;
                        }
                        min_distance = distance;
                        min_hp = enemy_unit->health;
                        target = enemy_unit;

                    }
                }
            }
            // 2nd Priority -> Workers
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(worker_types.begin(), worker_types.end(), enemy_unit->unit_type) != worker_types.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 3rd Priority -> Any units that are not structures
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
                    if (!is_structure &&
                        enemy_unit->unit_type != UNIT_TYPEID::ZERG_LARVA &&
                        enemy_unit->unit_type != UNIT_TYPEID::ZERG_EGG) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 4th Priority -> Supply structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(resource_units.begin(), resource_units.end(), enemy_unit->unit_type) != resource_units.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 5th Priority -> Any structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    bool is_structure = std::any_of(unit_type_data.attributes.begin(), unit_type_data.attributes.end(), [](Attribute attr) { return attr == Attribute::Structure; });

                    if (is_structure) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive) {
                                continue;
                            }
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

void BasicSc2Bot::Retreat(const Unit* unit) {

    if (unit == nullptr) {
        return;
    }

    Point2D nearest_corner;
    Point2D nearest_corner_ally;
    float min_corner_distance = std::numeric_limits<float>::max();;

    // Find nearest corner to the enemy start location
    for (const auto& corner : map_corners) {
        float corner_distance = DistanceSquared2D(unit->pos, corner);
        if (corner_distance < min_corner_distance) {
            min_corner_distance = corner_distance;
            nearest_corner = corner;
        }
    }

    // Reset min_corner_distance to find the nearest corner to the ally start location
    min_corner_distance = std::numeric_limits<float>::max();;
    for (const auto& corner : map_corners) {
        float corner_distance = DistanceSquared2D(start_location, corner);
        if (corner_distance < min_corner_distance) {
            min_corner_distance = corner_distance;
            nearest_corner_ally = corner;
        }
    }

    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D retreat_location(start_location.x + rx * 5.0f, start_location.y + ry * 5.0f);
    battlecruiser_retreat_location[unit] = retreat_location;


    // When the player and the enemy are in the same horizontal vertex
    if (nearest_corner_ally.x == nearest_corner.x) {
        Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, retreat_location);
        battlecruiser_retreating[unit] = true;
        enemy_location = 0;
    }

    // When the player and the enemy are in the same vertical vertex
    else if (nearest_corner_ally.y == nearest_corner.y) {
        Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, retreat_location);
        battlecruiser_retreating[unit] = true;
        enemy_location = 1;
    }

    // When the player and the enemy are in diagonal opposite
    else {
        Point2D adj_corner;
        enemy_location = 2;

        // Determine adjacent corner
        if (nearest_corner == map_corners[0]) {  // Bottom-left
            adj_corner = map_corners[1];         // Bottom-right
        }
        else if (nearest_corner == map_corners[1]) {  // Bottom-right
            adj_corner = map_corners[0];               // Bottom-left
        }
        else if (nearest_corner == map_corners[2]) {  // Top-left
            adj_corner = map_corners[3];               // Top-right
        }
        else if (nearest_corner == map_corners[3]) {  // Top-right
            adj_corner = map_corners[2];               // Top-left
        }


        Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, retreat_location);
        battlecruiser_retreating[unit] = true;
    }
}

void BasicSc2Bot::RetreatCheck() {
    // Distance threshold for arrival
    const float arrival_threshold = 5.0f;

    for (const auto& battlecruiser : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
        // Get the Battlecruiser's specific retreat location
        if (battlecruiser_retreating[battlecruiser]) {
            Point2D retreat_location = battlecruiser_retreat_location[battlecruiser];

            // Check if it has reached its retreat location and in full health
            if (Distance2D(battlecruiser->pos, retreat_location) <= arrival_threshold) {
                if (battlecruiser->orders.empty() && battlecruiser->health >= battlecruiser->health_max) {
                    battlecruiser_retreating[battlecruiser] = false;
                    battlecruiser_retreat_location.erase(battlecruiser);
                }
            }
        }
    }
}
