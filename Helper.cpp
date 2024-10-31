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
    // Determine if we need a new base
    const ObservationInterface* observation = Observation();

    // Get all bases
    Units bases = observation->GetUnits(Unit::Self, IsTownHall());
    if (bases.empty()) {
        return false; // No bases to expand from
    }

    // Consider expansion based on worker saturation and resource availability
    size_t total_workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV)).size();
    size_t total_mineral_patches = 0;
    size_t total_ideal_workers = 0;

    for (const auto& base : bases) {
        Units mineral_patches = observation->GetUnits(Unit::Alliance::Neutral, [base](const Unit& unit) {
            return unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD && Distance2D(unit.pos, base->pos) < 10.0f;
        });
        total_mineral_patches += mineral_patches.size();
        total_ideal_workers += (mineral_patches.size() * 2) + 6; // 2 workers per mineral patch + 6 for gas
    }

    // Expand if we are close to or at worker saturation and have fewer bases
    return total_workers >= 0.8 * total_ideal_workers; // Expand when close to saturation
}

Point3D BasicSc2Bot::GetNextExpansion() const {
    // Get the next available expansion location
    const ObservationInterface* observation = Observation();
    Units possible_expansions = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_MINERALFIELD));

    Point3D main_base_location = GetMainBase()->pos;
    float distance = std::numeric_limits<float>::max();
    Point3D next_expansion;

    for (const auto& expansion : possible_expansions) {
        float current_distance = DistanceSquared3D(expansion->pos, main_base_location);
        if (current_distance < distance) {
            distance = current_distance;
            next_expansion = expansion->pos;
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

bool BasicSc2Bot::IsMainBaseUnderAttack() {
    // If any structure in the main base is under attack, return true
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER ||
            unit->unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
            unit->unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS) {
            if (unit->health < unit->health_max) {
                return true;
            }
        }
    }
    return false;
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
