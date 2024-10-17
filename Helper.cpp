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
