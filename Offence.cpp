#include "BasicSc2Bot.h"

void BasicSc2Bot::Offense() {
    const ObservationInterface* observation = Observation();
    
    // Check if we should start attacking
    if (!is_attacking) {
        if (num_marines >= 30 && num_siege_tanks >= 1) {
            is_attacking = true;
        }
    }

    // Once we're attacking, keep up the pressure
    if (is_attacking) {
        // Check if our army is mostly dead
        Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
        Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
        
        // If army is severely depleted, rebuild before attacking again
        if (marines.size() < 10 && siege_tanks.empty()) {
            is_attacking = false;
        } else {
            AllOutRush();
        }
    }
}

void BasicSc2Bot::AllOutRush() {
    const ObservationInterface* observation = Observation();

    // Get all our combat units
    Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
    Units battlecruisers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

    // Determine attack target
    Point2D attack_target = enemy_start_location;

    // First check for visible enemy structures
    Units enemy_structures = observation->GetUnits(Unit::Alliance::Enemy, [this](const Unit& unit) {
        const UnitTypeData& type_data = Observation()->GetUnitTypeData()[unit.unit_type];
        return std::find(type_data.attributes.begin(), type_data.attributes.end(), Attribute::Structure) != type_data.attributes.end();
    });

    if (!enemy_structures.empty()) {
        // Find closest enemy structure to our army
        Point2D army_center(0, 0);
        int unit_count = marines.size() + siege_tanks.size() + battlecruisers.size();
        
        if (unit_count > 0) {
            for (const auto& marine : marines) army_center += marine->pos;
            for (const auto& tank : siege_tanks) army_center += tank->pos;
            for (const auto& bc : battlecruisers) army_center += bc->pos;
            army_center /= unit_count;

            float min_distance = std::numeric_limits<float>::max();
            for (const auto& structure : enemy_structures) {
                float distance = Distance2D(army_center, structure->pos);
                if (distance < min_distance) {
                    min_distance = distance;
                    attack_target = structure->pos;
                }
            }
        }
    } else {
        // Check expansion locations near the enemy base first
        for (const auto& exp : expansion_locations) {
            Point2D exp_point(exp.x, exp.y);
            if (observation->GetVisibility(exp_point) != Visibility::Hidden) {
                attack_target = exp_point;
                break;
            }
        }
    }

    // Move units to the target location
    for (const auto& bc : battlecruisers) {
        Actions()->UnitCommand(bc, ABILITY_ID::MOVE_MOVE, attack_target);
    }

    for (const auto& marine : marines) {
        Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, attack_target);
    }

    for (const auto& tank : siege_tanks) {
        Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, attack_target);
    }

    if (!is_attacking) {
        is_attacking = true;
    }
}