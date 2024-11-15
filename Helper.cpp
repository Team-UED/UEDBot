#include "BasicSc2Bot.h"

const Unit* BasicSc2Bot::GetMainBase() const {
    // Get the main Command Center, Orbital Command, or Planetary Fortress
    Units command_centers = Observation()->GetUnits(Unit::Self, IsTownHall());
    if (!command_centers.empty()) {
        return command_centers.front();
    }
    return nullptr;
}

bool BasicSc2Bot::NeedExpansion() const {
    const ObservationInterface* observation = Observation();

    Units all_bases = observation->GetUnits(Unit::Self, IsTownHall());
    Units bases;
    for (const auto& base : all_bases) {
        if (base->build_progress == 1.0f) {
            bases.push_back(base);
        }
    }


    if (bases.empty()) {
        return true; // Need to rebuild if all bases are lost
    }

    const size_t max_bases = 3; // Adjust this value as desired
    if (bases.size() >= max_bases) {
        return false; // Do not expand if we've reached the maximum number of bases
    }

    // Calculate total ideal workers
    size_t total_ideal_workers = 0;
    for (const auto& base : bases) {
        total_ideal_workers += base->ideal_harvesters;
    }

    // Get current number of SCVs
    size_t num_scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV)).size();

    // Expand when we have enough SCVs to saturate our current bases
    return num_scvs >= 0.95 * total_ideal_workers;
}


Point3D BasicSc2Bot::GetNextExpansion() const {
    const ObservationInterface* observation = Observation();

    // Check if we have enough resources to expand
    if (observation->GetMinerals() < 400) {
        return Point3D(0.0f, 0.0f, 0.0f);
    }

    const std::vector<Point3D>& expansions = expansion_locations;
    Units townhalls = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

	if (townhalls.empty() || GetMainBase() == nullptr) {
		return Point3D(0.0f, 0.0f, 0.0f);
	}

    // Find the closest unoccupied expansion location
    Point3D main_base_location = GetMainBase()->pos;
    float closest_distance = std::numeric_limits<float>::max();
    Point3D next_expansion = Point3D(0.0f, 0.0f, 0.0f);

    for (const auto& expansion_pos : expansions) {
        bool occupied = false;
        for (const auto& townhall : townhalls) {
            if (Distance2D(expansion_pos, townhall->pos) < 5.0f) {
                occupied = true;
                break;
            }
        }
        if (occupied) {
            continue;
        }
        float distance = DistanceSquared3D(expansion_pos, main_base_location);
        if (distance < closest_distance) {
            closest_distance = distance;
            next_expansion = expansion_pos;
        }
    }
    return next_expansion;
}

// Helper function to detect dangerous positions
bool BasicSc2Bot::IsDangerousPosition(const Point2D &pos) {
    // if enemy units are within a certain radius (run!!!!)
    auto enemy_units = Observation()->GetUnits(Unit::Alliance::Enemy);
    for (const auto &enemy : enemy_units) {
        if (Distance2D(pos, enemy->pos) < 10.0f) {
            return true;
        }
    }
    return false;
}

// Helper function to find a safe position for retreat
Point2D BasicSc2Bot::GetSafePosition() {
    // currently set up to return the location of command center
    const Unit *command_center = nullptr;
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER) {
            command_center = unit;
            break;
        }
    }
    return command_center ? command_center->pos : Point2D(0, 0);
}

// Helper function to find damaged units for repair
const Unit *BasicSc2Bot::FindDamagedUnit() {
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER &&
            unit->health < unit->health_max) {
            return unit;
        }
    }
    return nullptr;
}

// Helper function to find damaged structures for repair
const Unit *BasicSc2Bot::FindDamagedStructure() {
   for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
       if (unit->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER ||
           unit->unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
           unit->unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS ||
           unit->unit_type == UNIT_TYPEID::TERRAN_SUPPLYDEPOT ||
           unit->unit_type == UNIT_TYPEID::TERRAN_BARRACKS ||
           unit->unit_type == UNIT_TYPEID::TERRAN_FACTORY ||
           unit->unit_type == UNIT_TYPEID::TERRAN_STARPORT) {
           if (unit->health < unit->health_max) {
               return unit;
           }
       }
   }
   return nullptr;
}

bool BasicSc2Bot::IsWorkerUnit(const Unit* unit) {
    return unit->unit_type == UNIT_TYPEID::TERRAN_SCV ||
           unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE ||
           unit->unit_type == UNIT_TYPEID::ZERG_DRONE ||
           unit->unit_type == UNIT_TYPEID::TERRAN_MULE;
}

bool BasicSc2Bot::IsTrivialUnit(const Unit* unit) {
    return unit->unit_type == UNIT_TYPEID::ZERG_OVERLORD ||
        unit->unit_type == UNIT_TYPEID::ZERG_CHANGELING ||
        unit->unit_type == UNIT_TYPEID::ZERG_CHANGELINGMARINE ||
        unit->unit_type == UNIT_TYPEID::ZERG_CHANGELINGMARINESHIELD ||
        unit->unit_type == UNIT_TYPEID::PROTOSS_OBSERVER ||
        unit->unit_type == UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE;
}


// Modify IsMainBaseUnderAttack() to consider only combat units
bool BasicSc2Bot::IsMainBaseUnderAttack() { 
    const Unit* main_base = GetMainBase();
    if (!main_base) {
        return false;
    }

    // Check if there are enemy combat units near our main base
    Units enemy_units_near_base = Observation()->GetUnits(Unit::Alliance::Enemy, [this, main_base](const Unit& unit) {
        float distance = Distance2D(unit.pos, main_base->pos);
        if (distance < 25.0f && !IsWorkerUnit(&unit) && !IsTrivialUnit(&unit)) {
            return true;
        }
        return false;
    });

    return !enemy_units_near_base.empty();
}


// Helper function to find the closest enemy unit
const Unit *BasicSc2Bot::FindClosestEnemy(const Point2D &pos) {
    const Unit *closest_enemy = nullptr;
    float closest_distance = std::numeric_limits<float>::max();

    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
        float distance = Distance2D(pos, unit->pos);
        if (distance < closest_distance) {
            closest_distance = distance;
            closest_enemy = unit;
        }
    }
    return closest_enemy;
}

const Unit* BasicSc2Bot::FindUnit(sc2::UnitTypeID unit_type) const {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
    for (const auto& unit : units) {
        // Exclude SCVs that are currently constructing
        if (unit->orders.empty() || unit->orders.front().ability_id == ABILITY_ID::HARVEST_GATHER) {
            return unit;
        }
    }
    return nullptr;
}

bool BasicSc2Bot::TryBuildStructureAtLocation(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type, const Point2D& location) {
    const ObservationInterface* observation = Observation();
    const Unit* builder = FindUnit(unit_type);

    if (!builder) return false;

    if (Query()->Placement(ability_type_for_structure, location, builder)) {
        Actions()->UnitCommand(builder, ability_type_for_structure, location);
        return true;
    } else {
        // Try alternate locations near the initial location
        for (float x_offset = -5.0f; x_offset <= 5.0f; x_offset += 1.0f) {
            for (float y_offset = -5.0f; y_offset <= 5.0f; y_offset += 1.0f) {
                Point2D new_location = location + Point2D(x_offset, y_offset);
                if (Query()->Placement(ability_type_for_structure, new_location, builder)) {
                    Actions()->UnitCommand(builder, ability_type_for_structure, new_location);
                    return true;
                }
            }
        }
    }
    return false;
}

Point2D BasicSc2Bot::GetRallyPoint() {
    const Unit* main_base = GetMainBase();
    if (main_base) {
        // Adjust the rally point as needed
        return Point2D(main_base->pos.x + 15.0f, main_base->pos.y);
    }
    // Fallback position
    return Point2D(0.0f, 0.0f);
}

const Unit* BasicSc2Bot::GetLeastSaturatedBase() const {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

    const Unit* least_saturated_base = nullptr;
    int max_worker_need = 0;

    for (const auto& base : bases) {
        if (base->build_progress < 1.0f || base->is_flying) {
            continue; // Skip bases that are under construction or flying
        }

        int worker_need = base->ideal_harvesters - base->assigned_harvesters;
        if (worker_need > max_worker_need) {
            max_worker_need = worker_need;
            least_saturated_base = base;
        }
    }

    return least_saturated_base;
}

Point2D BasicSc2Bot::GetChokepointPosition() {
    // Get the main base location
    const Unit* main_base = GetMainBase();
    if (!main_base) {
        return Point2D(0.0f, 0.0f);
    }

    // Find the closest chokepoint to the main base
    float closest_distance = std::numeric_limits<float>::max();
    Point2D chokepoint_position = Point2D(0.0f, 0.0f);

    for (const auto& chokepoint : chokepoints) {
        float distance = Distance2D(main_base->pos, chokepoint);
        if (distance < closest_distance) {
            closest_distance = distance;
            chokepoint_position = chokepoint;
        }
    }

    return chokepoint_position;
}

bool BasicSc2Bot::IsAnyBaseUnderAttack() {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    for (const auto& base : bases) {
        if (base->health < base->health_max) {
            return true;
        }
    }
    return false;
}