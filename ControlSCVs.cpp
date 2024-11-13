//Other than gathering resources,
//    SCVs must
//
//            Retreat from dangerous
//            situations(e.g.,
//                       enemy rushes or harass) to avoid resource loss deaths.
//
//        Repair damaged Battlecruisers during
//        or after engagements
//               .
//
//           Repair structures during enemy attacks to maintain defenses.
//
//           Attack in some urgent situations
//            (e.g., when the enemy is attacking the main base).


#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control SCVs
void BasicSc2Bot::ControlSCVs() {
	SCVScout();
    RetreatFromDanger();
    RepairUnits();
    RepairStructures();
    SCVAttackEmergency();
}

// SCVs scout the map to find enemy bases
void BasicSc2Bot::SCVScout() {
    const sc2::ObservationInterface* observation = Observation();

    // Check if we have enough SCVs
    sc2::Units scvs = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));
    if (scvs.size() < 10 || scout_complete) {
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
            if (distance_to_target < 2.0f) {
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
                    if (sc2::Distance2D(scout_location, structure->pos) < 2.0f) {
                        // Set the enemy start location and stop scouting
                        enemy_start_location = structure->pos;
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

// SCVs retreat from dangerous situations (e.g., enemy rushes)
void BasicSc2Bot::RetreatFromDanger() {
    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV && unit != scv_scout) {
            if (IsDangerousPosition(unit->pos)) {
                Actions()->UnitCommand(unit, ABILITY_ID::SMART,
                    GetSafePosition());
            }
        }
    }
}

// SCVs repair damaged Battlecruisers during or after engagements
void BasicSc2Bot::RepairUnits() {
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
            const Unit *target = FindDamagedUnit();
            if (target) {
                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_REPAIR, target);
            }
        }
    }
}

// SCVs repair damaged structures during enemy attacks
void BasicSc2Bot::RepairStructures() {
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
            const Unit *target = FindDamagedStructure();
            if (target) {
                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_REPAIR, target);
            }
        }
    }
}

// SCVs attack in urgent situations (e.g., enemy attacking the main base)
void BasicSc2Bot::SCVAttackEmergency() {
    if (IsMainBaseUnderAttack()) {
        // Get enemy units near our main base
        Units enemy_units_near_base = Observation()->GetUnits(Unit::Alliance::Enemy, [this](const Unit& unit) {
            // Return enemy units that are near our main base and are combat units
            const Unit* main_base = GetMainBase();
            if (main_base) {
                return Distance2D(unit.pos, main_base->pos) < 15.0f && !IsWorkerUnit(&unit);
            }
            return false;
        });

        // If there are significant enemy combat units, send SCVs to attack
        if (!enemy_units_near_base.empty()) {
            int scvs_sent = 0;
            const int max_scvs_to_send = 5; // Limit the number of SCVs
            for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
                if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
                    const Unit* target = FindClosestEnemy(unit->pos);
                    if (target && !IsWorkerUnit(target)) {
                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, target);
                        scvs_sent++;
                        if (scvs_sent >= max_scvs_to_send) {
                            break;
                        }
                    }
                }
            }
        }
    }
}