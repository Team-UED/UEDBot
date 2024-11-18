
#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Battlecruisers
void BasicSc2Bot::ControlBattlecruisers() {
	Jump();
	TargetBattlecruisers();
    RetreatCheck();
}

// Use Tactical Jump into enemy base
void BasicSc2Bot::Jump() {

    // Check if the main base is under attack; don't use Tactical Jump in that case
    if (IsMainBaseUnderAttack()) {
        return;
    }

    // Check if any Battlecruiser is still retreating
    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && battlecruiser_retreating[unit]) {
            // Wait until all retreating Battlecruisers finish their retreat
            return;
        }
    }

    
    // No retreating Battlecruisers, proceed with Tactical Jump logic
    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        // Check if the unit is a Battlecruiser with full health and not retreating
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER &&
            unit->health >= unit->health_max &&
            Distance2D(unit->pos, enemy_start_location) > 40.0f &&
            HasAbility(unit, ABILITY_ID::EFFECT_TACTICALJUMP)) {
            Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_TACTICALJUMP, enemy_start_location);
        }
    }
}


/// Target enemy units based on threat levels
void BasicSc2Bot::TargetBattlecruisers() {

    // Detect radius for Battlecruisers
    const float defense_check_radius = 14.0f;

	// Maximum distance to consider for targetting
	const float max_distace_for_target = 40.0f;

    // Get Battlecruisers
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
            
			// Only count combat units as enemies
            if (IsWorkerUnit(enemy_unit) || IsTrivialUnit(enemy_unit)) {
                continue; 
            }

            float distance_to_enemy = Distance2D(battlecruiser->pos, enemy_unit->pos);
            if (distance_to_enemy <= 15.0f) {
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

    std::set<Tag> yamato_targets;

    for (const auto& battlecruiser : battlecruisers) {
        // Disables targetting while Jumping
        if (!battlecruiser->orders.empty()) {
            const AbilityID current_ability = battlecruiser->orders.front().ability_id;
            if (current_ability == ABILITY_ID::EFFECT_TACTICALJUMP) {
                continue;
            }
        }

        int total_threat = 0;
        const Unit* target = nullptr;
        constexpr float max_distance = std::numeric_limits<float>::max();
        float min_distance = max_distance;
        float min_hp = std::numeric_limits<float>::max();

        // Dynamically set threat levels for enemy units
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
        if (total_threat >= threat_threshold && (total_threat != 0) && (threat_threshold != 0)) {

            float health_threshold = 0.0f;

            // Extremly Strong anti air units -> Retreat when hp = 400
            if (total_threat >= 2.0 * threat_threshold) {
                health_threshold = 450.0f;
            }
            // Strong anti air units -> Retreat when hp = 400
            else if (total_threat >= 1.5 * threat_threshold && total_threat <= 2.0 * threat_threshold) {
                health_threshold = 350.0f;
            }
            // Moderate anti air units -> Retreat when hp = 200
            else if (total_threat > threat_threshold && total_threat <= 1.5 * threat_threshold) {
                health_threshold = 250.0f;
            }

            if (battlecruiser->health <= health_threshold) {
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

                // Kite enemy units
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
                    float kiting_distance = 8.0f;


                    // Calculate the avoidance direction from the enemy vertex
                    Point2D avoidance_direction = battlecruiser->pos - nearest_corner;
                    float avoidance_length = std::sqrt(avoidance_direction.x * avoidance_direction.x + avoidance_direction.y * avoidance_direction.y);
                    if (avoidance_length > 0) {
                        avoidance_direction /= avoidance_length;
                    }

                    // Define weights for combining directions
                    // Weight for kiting direction
                    float kite_weight = 0.3f;
                    // Weight for avoiding enemy vertex
                    float avoidance_weight = 0.7f;

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
			// Retreat Immediately if the Battlecruiser is below 150 health
            if ((battlecruiser->health <= 150.0f)) {
                Retreat(battlecruiser);
                return;
            }

			UseYamatoCannon(battlecruiser, Observation()->GetUnits(Unit::Alliance::Enemy), yamato_targets);

			// Calculate turret status
            const Unit* turret_nearest = nullptr;
            float min_distance = max_distance;
            int num_turrets = 0;

            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                if (std::find(turret_types.begin(), turret_types.end(), enemy_unit->unit_type) != turret_types.end()) {
                    num_turrets++;
                    float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                    if (distance < min_distance) {
                        if (!enemy_unit->is_alive) {
                            continue;
                        }
                        turret_nearest = enemy_unit;
                    }
                }
            }

            // 1st Priority -> Attacking enemy units
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                auto threat = threat_levels.find(enemy_unit->unit_type);
                if (threat != threat_levels.end()) {
                    if (enemy_unit->unit_type == UNIT_TYPEID::ZERG_SPORECRAWLER &&
                        enemy_unit->unit_type == UNIT_TYPEID::TERRAN_MISSILETURRET &&
                        enemy_unit->unit_type == UNIT_TYPEID::PROTOSS_PHOTONCANNON) {

                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
                                continue;
                            }
                            if (num_turrets <= 2 * num_battlecruisers_in_combat && 
                                total_threat - (3 * num_turrets) == 0) {
                                min_distance = distance;
                                min_hp = enemy_unit->health;
                                target = enemy_unit;
                            }
                        }
                    }
                    else {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 2nd Priority -> Workers
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(worker_types.begin(), worker_types.end(), enemy_unit->unit_type) != worker_types.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 3rd Priority -> Turrets
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(turret_types.begin(), turret_types.end(), enemy_unit->unit_type) != turret_types.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (enemy_unit->health < min_hp || (enemy_unit->health == min_hp && distance < min_distance)) {
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 4th Priority -> Any units that are not structures
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
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 5th Priority -> Supply structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(resource_units.begin(), resource_units.end(), enemy_unit->unit_type) != resource_units.end()) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
                                continue;
                            }
                            min_distance = distance;
                            min_hp = enemy_unit->health;
                            target = enemy_unit;
                        }
                    }
                }
            }

            // 6th Priority -> Any structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    bool is_structure = std::any_of(unit_type_data.attributes.begin(), unit_type_data.attributes.end(), [](Attribute attr) { return attr == Attribute::Structure; });

                    if (is_structure) {
                        float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                        if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                            if (!enemy_unit->is_alive || distance > max_distace_for_target) {
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
            if (target && target->NotCloaked) {
				// No turret nearby or turret is the target or there are only turrets in threat radius -> Attack
                if (turret_nearest == nullptr || 
                    std::find(turret_types.begin(), turret_types.end(), target->unit_type) != turret_types.end()) {
                    Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, target);
				}
                else {
					// Turret exists but far away from the Battlecruiser -> Attack the target
                    if (Distance2D(battlecruiser->pos, turret_nearest->pos) >= 10.0f) {
                        Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, target);
                    }
					// Turret exists and near Battlecruiser -> Kite to safe position
                    else {
                        float distance_to_target = Distance2D(battlecruiser->pos, turret_nearest->pos);

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
                        Point2D kite_direction = battlecruiser->pos - turret_nearest->pos;

                        // Normalize the kite direction vector
                        float kite_length = std::sqrt(kite_direction.x * kite_direction.x + kite_direction.y * kite_direction.y);
                        if (kite_length > 0) {
                            kite_direction /= kite_length;
                        }

                        // Define the kiting distance 
                        float kiting_distance = 9.0f;

                        // Calculate the avoidance direction from the enemy vertex
                        Point2D avoidance_direction = battlecruiser->pos - nearest_corner;
                        float avoidance_length = std::sqrt(avoidance_direction.x * avoidance_direction.x + avoidance_direction.y * avoidance_direction.y);
                        if (avoidance_length > 0) {
                            avoidance_direction /= avoidance_length;
                        }

                        // Define weights for combining directions
                        // Weight for kiting direction
                        float kite_weight = 0.5f;
                        // Weight for avoiding enemy vertex
                        float avoidance_weight = 0.5f;

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
            // No target
            else {
                if (battlecruiser->health >= 500.0f) {
                    if (Distance2D(battlecruiser->pos, enemy_start_location) < 40.0f) {
                        Retreat(battlecruiser);
                        return;
                    }
                    else {
                        // Check if any Battlecruiser is still retreating
                        for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
                            if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && battlecruiser_retreating[unit]) {
                                // Wait until all retreating Battlecruisers finish their retreat
                                return;
                            }
                        }
                        Actions()->UnitCommand(battlecruiser, ABILITY_ID::EFFECT_TACTICALJUMP, enemy_start_location);
                    }
                }
				else {
                    Retreat(battlecruiser);
                    return;
				}
            }
        }
    }
}

void BasicSc2Bot::Retreat(const Unit* unit) {

    if (unit == nullptr) {
        return;
    }

    battlecruiser_retreat_location[unit] = retreat_location;
    battlecruiser_retreating[unit] = true;
    Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, retreat_location);
}

void BasicSc2Bot::RetreatCheck() {
    // Distance threshold for arrival
    const float arrival_threshold = 5.0f;

    for (const auto& battlecruiser : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
		// Check if the Battlecruiser has reached the retreat location
        if (battlecruiser_retreating[battlecruiser] &&
            Distance2D(battlecruiser->pos, retreat_location) <= arrival_threshold &&
            battlecruiser->health >= 550.0f) {
            battlecruiser_retreat_location.erase(battlecruiser);
            battlecruiser_retreating[battlecruiser] = false;
        }
    }
}

void BasicSc2Bot::UseYamatoCannon(const Unit* battlecruiser, const Units& enemy_units, std::set<Tag>& yamato_targets) {

    // Skip if Yamato Cannon is not available
    if (!HasAbility(battlecruiser, ABILITY_ID::EFFECT_YAMATOGUN)) {
        return; 
    }

    const int yamato_damage = 240;       // Yamato Cannon damage
    const float yamato_range = 10.0f;    // Yamato Cannon range
    const int low_health_threshold = 80; // Skip units with low health

    const Unit* target = nullptr;
    int highest_priority = 0;            
    int oneshot = std::numeric_limits<int>::max(); 

    for (const auto& enemy_unit : enemy_units) {
        // Skip already targeted units
        if (yamato_targets.find(enemy_unit->tag) != yamato_targets.end()) {
            continue;
        }

        // Skip if the unit is dead or has very low health
        if (!enemy_unit->is_alive || enemy_unit->health < low_health_threshold) {
            continue;
        }

        // Check if the unit is within Yamato Cannon range
        if (Distance2D(battlecruiser->pos, enemy_unit->pos) > yamato_range) {
            continue;
        }

        // Determine threat level of the enemy unit
        auto threat = threat_levels.find(enemy_unit->unit_type);
        if (threat == threat_levels.end()) {
            continue; 
        }
        int threat_level = threat->second;

        // Exclude low value units
        if (enemy_unit->unit_type == UNIT_TYPEID::TERRAN_MARINE ||
            enemy_unit->unit_type == UNIT_TYPEID::ZERG_HYDRALISK ||
			enemy_unit->unit_type == UNIT_TYPEID::PROTOSS_SENTRY ||
			enemy_unit->unit_type == UNIT_TYPEID::ZERG_MUTALISK ||
			enemy_unit->unit_type == UNIT_TYPEID::ZERG_RAVAGER ||
            enemy_unit->unit_type == UNIT_TYPEID::TERRAN_GHOST) {
            continue;
        }

        // Prioritize turrets and queens (if they are not low health)
        bool is_high_value_target =
            enemy_unit->unit_type == UNIT_TYPEID::TERRAN_MISSILETURRET ||
            enemy_unit->unit_type == UNIT_TYPEID::PROTOSS_PHOTONCANNON ||
            enemy_unit->unit_type == UNIT_TYPEID::ZERG_SPORECRAWLER ||
            enemy_unit->unit_type == UNIT_TYPEID::ZERG_QUEEN;

        // Immediately target high-value units
        if (is_high_value_target && enemy_unit->health > low_health_threshold) {
            target = enemy_unit;
            break; 
        }

        // For other units, calculate how close the health is to Yamato Cannon's damage
        int hp_difference = std::abs(static_cast<int>(enemy_unit->health) - yamato_damage);

        // Prioritize:
        // 1. Units closest to Yamato damage (one-shottable).
        // 2. Among equally one-shottable units, prefer those with higher threat levels.
        if (hp_difference < oneshot || (hp_difference == oneshot && threat_level > highest_priority)) {
            oneshot = hp_difference;
            highest_priority = threat_level;
            target = enemy_unit;
        }
    }

    // If a valid target is found, use Yamato Cannon and mark the target
    if (target) {
        Actions()->UnitCommand(battlecruiser, ABILITY_ID::EFFECT_YAMATOGUN, target);
        yamato_targets.insert(target->tag);
    }
}