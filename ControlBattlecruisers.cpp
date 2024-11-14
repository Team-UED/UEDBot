
#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Battlecruisers
void BasicSc2Bot::ControlBattlecruisers() {
	Jump();
	Target();
    Retreat_check();
}

// Use Tactical Jump into enemy base
void BasicSc2Bot::Jump() {

    const float base_radius = 30.0f;

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
            if (distance < base_radius) {
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
					std::cout << "Tactical Jump!" << std::endl;
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

    // Distance between battlecruisers
    const float nearby_distance_threshold = 20.0f;

    // Distance from enemy
    const float max_distance_from_enemy = 15.0f;

    // Dynamically set threat levels for enemy units
    const Units battlecruisers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

    // Exit when there are no battlecruisers
    if (battlecruisers.empty()) {
        return;
    }

	// Number of Battlecruisers in combat
    int num_battlecruisers_in_combat= 0;

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
            std::cout << "disable targeting since retreating" << std::endl;
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


        // Determine whether to kite based on the threat level
        // Kite if the total threat level is above the threshold
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
            // 

            if (battlecruiser->health <= health_threshold) {
				Retreat(battlecruiser);
            }
            
        }
        // Do not kite if the total threat level is below the threshold
        else {

            // 1st Priority -> Attacking enemy units
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
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
            std::cout << "targeting enemy unit" << std::endl;
            // 2nd Priority -> Workers
            if (!target) {
                std::vector<UNIT_TYPEID> worker_types = {
                    UNIT_TYPEID::TERRAN_SCV,
                    UNIT_TYPEID::PROTOSS_PROBE,
                    UNIT_TYPEID::ZERG_DRONE
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
                std::cout << "targeting workers" << std::endl;
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
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
                std::cout << "targeting any units" << std::endl;
            }

            // 4th Priority -> Supply structures
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
                std::cout << "targeting supply structures" << std::endl;
            }

            // 5th Priority -> Any structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    bool is_structure = std::any_of(unit_type_data.attributes.begin(), unit_type_data.attributes.end(), [](Attribute attr) { return attr == Attribute::Structure; });

                    if (is_structure) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
                std::cout << "targeting structures" << std::endl;
            }
            // Attack the selected target
            if (target) {
                Actions()->UnitCommand(battlecruiser, ABILITY_ID::ATTACK_ATTACK, target);
            }
        }
    }
}

void BasicSc2Bot::Retreat(const Unit* unit) {

    std::cout << "Retreat!!" << std::endl;

	if (unit == nullptr) {
		return;
	}
    Point2D nearest_corner;
    Point2D nearest_corner_ally;
    float min_corner_distance = std::numeric_limits<float>::max();;

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
        Actions()->UnitCommand(unit, ABILITY_ID::SMART, retreat_location);
        battlecruiser_retreating[unit] = true;
        enemy_location = 0;
    }

    // When the player and the enemy are in the same vertical vertex
    else if (nearest_corner_ally.y == nearest_corner.y) {
        Actions()->UnitCommand(unit, ABILITY_ID::SMART, retreat_location);
        battlecruiser_retreating[unit] = true;
        enemy_location = 1;
    }

    // When the player and the enemy are in diagonal opposite
    else {
        std::cout << "diagonal opposite" << std::endl;
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

void BasicSc2Bot::Retreat_check() {
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
