#include "BasicSc2Bot.h"

// Main function to control SCVs
void BasicSc2Bot::ControlSCVs() {
    SCVScoutEnemySpawn();
    RetreatFromDanger();
	UpdateRepairingSCVs();
    RepairUnits();
    RepairStructures();
    UpdateRepairingSCVs();
    SCVAttackEmergency();
}

// SCVs scout the map to find enemy bases
void BasicSc2Bot::SCVScoutEnemySpawn() {
    const sc2::ObservationInterface* observation = Observation();

    // Check if we have enough SCVs
    sc2::Units scvs = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));

	if (scvs.empty() || scvs.size() < 12 || scout_complete) {
		return;
	}

	// Check if enemy start locations are available
    if (enemy_start_locations.empty()) {
        scout_complete = true;  
        return;
    }

    if (is_scouting) {
        // Get scouting SCV
        scv_scout = observation->GetUnit(scv_scout->tag);

        if (scv_scout) {
            // Update the scouting SCV's current locationcd.
            scout_location = scv_scout->pos;

            // Check if SCV has reached the current target location
            float distance_to_target = sc2::Distance2D(scout_location, enemy_start_locations[current_scout_location_index]);
            if (distance_to_target < 5.0f) {
                // Check for enemy town halls
                sc2::Units enemy_structures = observation->GetUnits(sc2::Unit::Alliance::Enemy, [](const sc2::Unit& unit) {
                    return unit.unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
                        unit.unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
                        unit.unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING ||
                        unit.unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING ||
                        unit.unit_type == sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS ||
                        unit.unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY ||
                        unit.unit_type == sc2::UNIT_TYPEID::ZERG_LAIR ||
                        unit.unit_type == sc2::UNIT_TYPEID::ZERG_HIVE ||
                        unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS;
                });

                for (const auto& structure : enemy_structures) {
                    if (sc2::Distance2D(scout_location, structure->pos) < 5.0f) {
                        // Set the enemy start location and stop scouting
                        enemy_start_location = structure->pos;


                        // Find the nearest corner to the enemy base
                        float min_corner_distance = std::numeric_limits<float>::max();

                        for (const auto& corner : map_corners) {
                            float corner_distance = DistanceSquared2D(enemy_start_location, corner);
                            if (corner_distance < min_corner_distance) {
                                min_corner_distance = corner_distance;
                                nearest_corner_enemy = corner;
                            }
                        }

                        // Find the corners adjacent to the enemy base
                        for (const auto& corner : map_corners) {
                            if (corner.x == nearest_corner_enemy.x || corner.y == nearest_corner_enemy.y) {
                                enemy_adjacent_corners.push_back(corner);
                            }
                        }

                        const ObservationInterface* observation = Observation();
                        sc2::Units mineral_patches = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_MINERALFIELD));

                        // Find the closest mineral patch to the start location
                        const sc2::Unit* closest_mineral = nullptr;
                        float min_distance = std::numeric_limits<float>::max();

                        for (const auto& mineral : mineral_patches) {
                            float distance = sc2::Distance2D(start_location, mineral->pos);
                            if (distance < min_distance) {
                                min_distance = distance;
                                closest_mineral = mineral;
                            }
                        }

                        // harvest mineral if a mineral patch is found
                        if (closest_mineral && scv_scout) {
                            Actions()->UnitCommand(scv_scout, ABILITY_ID::HARVEST_GATHER, closest_mineral);
                        }

                        // Mark scouting as complete
                        scv_scout = nullptr;
                        is_scouting = false;
                        scout_complete = true;
                        return;
                    }
                }

                // Move to the next potential enemy location if no town hall is found here
                current_scout_location_index++;
                // All locations have been checked. Mark scouting as complete
                if (current_scout_location_index >= enemy_start_locations.size()) {
                    scv_scout = nullptr;
                    is_scouting = false;
                    scout_complete = true;
                    return;
                }
                // Scout to the next location
                Actions()->UnitCommand(scv_scout, sc2::ABILITY_ID::MOVE_MOVE, enemy_start_locations[current_scout_location_index]);
            }
        }
    }
    else {
        // Assign an SCV to scout when no SCVs are scouting
        for (const auto& scv : scvs) {
            if (scv->orders.empty()) {
                scv_scout = scv;
                is_scouting = true;
                current_scout_location_index = 0;  // Start from the first location

                // Set the initial position of the scouting SCV
                scout_location = scv->pos;

                // Command SCV to move to the initial possible enemy location
                Actions()->UnitCommand(scv_scout, sc2::ABILITY_ID::MOVE_MOVE, enemy_start_locations[current_scout_location_index]);
                break;
            }
        }
    }
}

void BasicSc2Bot::SCVScoutMapInit() {
    // Get the observation interface and SCV units
    const sc2::ObservationInterface *observation = Observation();
    sc2::Units scvs = observation->GetUnits(
        sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));

    if (scvs.empty()) {
        return; // No SCVs available for scouting
    }

    // Use the first available SCV to scout and set it to scv_scout
    scv_scout = scvs.front();

    // Get map dimensions
    const GameInfo &game_info = observation->GetGameInfo();
    unsigned int width = game_info.width;
    unsigned int height = game_info.height;

    // Generate grid points across the entire map for scouting
    scout_points.clear(); // Clear previous scout points if any
    const unsigned int step_size =
        20; // Step size for grid points (adjust for thoroughness)
    for (unsigned int x = 0; x < width; x += step_size) {
        for (unsigned int y = 0; y < height; y += step_size) {
            Point2D grid_point(static_cast<float>(x), static_cast<float>(y));
            if (observation->IsPathable(grid_point)) {
                scout_points.push_back(grid_point);
            }
        }
    }

    // Start scouting from the beginning
    current_scout_index = 0;
    is_scouting = true;
}

void BasicSc2Bot::UpdateSCVScouting() {
    if (!is_scouting || !scv_scout) {
        return; // Scouting is not active or no SCV available
    }

    // Get the observation interface
    const sc2::ObservationInterface *observation = Observation();

    // Check if the SCV is still alive
    scv_scout = observation->GetUnit(scv_scout->tag);
    if (!scv_scout) {
        is_scouting = false; // SCV is dead, stop scouting
        return;
    }

    // Check if SCV has reached the current scout point
    if (current_scout_index < scout_points.size()) {
        Point2D current_target = scout_points[current_scout_index];

        if (scv_scout->orders.empty() ||
            sc2::Distance2D(scv_scout->pos, current_target) < 2.0f) {
            // SCV has reached the current target, move to the next point
            current_scout_index++;
        }

        // If there are more points to scout, command the SCV to move to the
        // next point
        if (current_scout_index < scout_points.size()) {
            Actions()->UnitCommand(scv_scout, sc2::ABILITY_ID::MOVE_MOVE,
                                   scout_points[current_scout_index]);
        }
    } else {
        // Scouting complete
        is_scouting = false;
    }

    // Check for enemy units or structures at the current location
    sc2::Units enemy_units_found = observation->GetUnits(
        sc2::Unit::Alliance::Enemy, [](const sc2::Unit &unit) {
            return unit.display_type == sc2::Unit::DisplayType::Visible;
        });

    for (const auto &enemy : enemy_units_found) {
        enemy_units.push_back(enemy); // Add enemy to the enemy_units vector
    }
}

// SCVs retreat from dangerous situations (e.g., enemy rushes)
void BasicSc2Bot::RetreatFromDanger() {
    // Iterate through all our units
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        // Only consider SCVs that are not the scouting SCV
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV && unit != scv_scout) {

            // Skip SCVs that are actively attacking
            bool scv_is_attacking = false; // Renamed to avoid conflict
            for (const auto &order : unit->orders) {
                if (order.ability_id == ABILITY_ID::ATTACK) {
                    scv_is_attacking = true;
                    break;
                }
            }
            if (scv_is_attacking || scvs_repairing.find(unit->tag) != scvs_repairing.end()) {
                continue; // Skip SCVs that are currently attacking, or reparing
            }

            // If the SCV is in a dangerous position, make it retreat
            if (IsDangerousPosition(unit->pos)) {
                Point2D safe_position = GetNearestSafePosition(unit->pos);
                Actions()->UnitCommand(unit, ABILITY_ID::SMART, safe_position);
            }
        }
    }
}


// SCVs repair damaged Battlecruisers during or after engagements
void BasicSc2Bot::RepairUnits() {
    
    // Radius around the base considered "at base".
    const float base_radius = 15.0f; 
    const Unit* target = FindDamagedUnit();

    if (target) {
        for (const auto& scv_tag : scvs_repairing) {

            const Unit* scv = Observation()->GetUnit(scv_tag);

            // Skip if the SCV is invalid
            if (!scv || !scv->is_alive) {
                continue;
            }

            // Check if SCV is already repairing
            bool is_repairing = false;
            for (const auto& order : scv->orders) {
                if (order.ability_id == ABILITY_ID::EFFECT_REPAIR || order.ability_id == ABILITY_ID::EFFECT_REPAIR_SCV) {
                    is_repairing = true;
                    break;
                }
            }

            // Skip SCV if it is already repairing
            if (is_repairing) {
                continue; 
            }

            bool is_at_base = sc2::Distance2D(target->pos, start_location) <= base_radius;
            if (is_at_base) {
                Actions()->UnitCommand(scv, ABILITY_ID::EFFECT_REPAIR, target);
            }
        }
    }
}

// SCVs repair damaged structures during enemy attacks
void BasicSc2Bot::RepairStructures() {
    const Unit* target = FindDamagedStructure();

    if (target) {
        for (const auto& scv_tag : scvs_repairing) {

            const Unit* scv = Observation()->GetUnit(scv_tag);

            // Skip if the SCV is invalid
            if (!scv || !scv->is_alive) {
                continue;
            }

            // Check if SCV is already repairing
            bool is_repairing = false;
            for (const auto& order : scv->orders) {
                if (order.ability_id == ABILITY_ID::EFFECT_REPAIR || order.ability_id == ABILITY_ID::EFFECT_REPAIR_SCV) {
                    is_repairing = true;
                    break;
                }
            }

            // Skip SCV if it is already repairing
            if (!is_repairing) {
                Actions()->UnitCommand(scv, ABILITY_ID::EFFECT_REPAIR, target);
            }
        }
    }
}

void BasicSc2Bot::UpdateRepairingSCVs() {
    for (const auto& scv_tag : scvs_repairing) {
        const Unit* scv = Observation()->GetUnit(scv_tag);

        // Skip dead or invalid SCVs
        if (!scv || !scv->is_alive) {
            continue; 
        }

		// Return SCVs to harvest when repair is complete
        if (!scv->orders.empty()) {
			// Check if the SCV is repairing
            const auto& order = scv->orders.front();
            if (order.ability_id == ABILITY_ID::EFFECT_REPAIR ||
                order.ability_id == ABILITY_ID::EFFECT_REPAIR_SCV) {
				// Check if the repair target is fully repaired
                const Unit* target = Observation()->GetUnit(order.target_unit_tag);
                if (target && target->health == target->health_max) {

                    // Find all refineries
					Units refineries = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
                    
                    // Find a refinery with fewer than 3 workers
                    const Unit* target_refinery = nullptr;
                    if (!refineries.empty()) {
                        for (const auto& refinery : refineries) {
                            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                                target_refinery = refinery;
                                break;
                            }
                        }
                    }

                    // Assign the SCV to the refinery if found
                    if (target_refinery) {
                        Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, target_refinery);
                        continue;
                    }

                    // Otherwise, find the closest mineral patch
                    Units mineral_patches = Observation()->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_MINERALFIELD));
                    const Unit* closest_mineral = nullptr;
                    float min_distance = std::numeric_limits<float>::max();

                    for (const auto& mineral : mineral_patches) {
                        float distance = sc2::Distance2D(start_location, mineral->pos);
                        if (distance < min_distance) {
                            min_distance = distance;
                            closest_mineral = mineral;
                        }
                    }

                    // Assign the SCV to harvest minerals if a mineral patch is found
                    if (closest_mineral) {
                        Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, closest_mineral);
                    }
                }
            }
        }
    }
}


// SCVs attack in urgent situations (e.g., enemy attacking the main base)
void BasicSc2Bot::SCVAttackEmergency() {
    if (IsMainBaseUnderAttack()) {
        // Get enemy units near our main base
        Units enemy_units_near_base = Observation()->GetUnits(
            Unit::Alliance::Enemy, [this](const Unit &unit) {
                // Return enemy units that are near our main base and are combat
                // units
                const Unit *main_base = GetMainBase();
                if (main_base) {
                    return Distance2D(unit.pos, main_base->pos) < 15.0f &&
                           !IsWorkerUnit(&unit);
                }
                return false;
            });

        // If there are significant enemy combat units, send SCVs to attack
        if (!enemy_units_near_base.empty()) {
            int scvs_sent = 0;
            const int max_scvs_to_send = 5; // Limit the number of SCVs
            const float max_distance_from_base =
                20.0f; // Maximum distance SCVs should go from the base

            const Unit *main_base = GetMainBase();
            if (!main_base) {
                return; // If we don't have a main base, exit
            }

            for (const auto &unit :
                 Observation()->GetUnits(Unit::Alliance::Self)) {
                if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
                    // Ensure SCV does not move too far from the main base
                    if (Distance2D(unit->pos, main_base->pos) >
                        max_distance_from_base) {
                        continue; // Skip this SCV if it is too far from the
                                  // base
                    }

                    // Find the closest enemy unit to attack
                    const Unit *target = FindClosestEnemy(unit->pos);
                    if (target && !IsWorkerUnit(target) &&
                        Distance2D(target->pos, main_base->pos) <
                            max_distance_from_base) {
                        scvs_sent++;
                        if (scvs_sent > max_scvs_to_send) {
                            break;
                        }
                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK,
                                               target);
                    }
                }
            }
        }
    }
}
