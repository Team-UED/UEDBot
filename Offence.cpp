#include "BasicSc2Bot.h"

void BasicSc2Bot::Offense() {
    const ObservationInterface* observation = Observation();
    
    // Check if we should start attacking
    if (!is_attacking) {
        if (num_marines >= 10 && num_siege_tanks >= 1) {
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

	// Check if we have any units to attack with
	if (marines.empty() && siege_tanks.empty()) {
		return;
	}

    // Determine attack target (Default target)
    Point2D attack_target = enemy_start_location;

    // Check for enemy presence at the attack target
    bool enemy_base_destroyed = true;

    // Check for enemy units or structures near the attack target, including snapshots
    for (const auto& enemy_unit : observation->GetUnits(Unit::Alliance::Enemy)) {
        if ((enemy_unit->display_type == Unit::DisplayType::Visible || enemy_unit->display_type == Unit::DisplayType::Snapshot) &&
            enemy_unit->is_alive &&
            Distance2D(enemy_unit->pos, attack_target) < 25.0f) {
            enemy_base_destroyed = false;
            break;
        }
    }

    if (enemy_base_destroyed) {

        const Unit* closest_unit = nullptr;
        float min_distance = std::numeric_limits<float>::max();

        // Search for any visible unit
        for (const auto& enemy_unit : observation->GetUnits(Unit::Alliance::Enemy)) {
            if (enemy_unit->display_type == Unit::DisplayType::Visible && enemy_unit->is_alive) {
                float distance = Distance2D(enemy_unit->pos, start_location);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_unit = enemy_unit;
                }
            }
        }

        // If a unit is found, set it as the new attack target
        if (closest_unit) {
            attack_target = closest_unit->pos;
        }
        else {
            // Search for the closest snapshot unit
            for (const auto& enemy_unit : observation->GetUnits(Unit::Alliance::Enemy)) {
                if (enemy_unit->display_type == Unit::DisplayType::Snapshot && enemy_unit->is_alive) {
                    float distance = Distance2D(enemy_unit->pos, start_location);
                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_unit = enemy_unit;
                    }
                }
            }
            // If a snapshot unit is found, set it as the new attack target
            if (closest_unit) {
                attack_target = closest_unit->pos;
            }
            // When there are no snapshot units
            else {
                return;
            }
        }
    }

    // Move units to the target location
    for (const auto& marine : marines) {
        if (marine->orders.empty() && Distance2D(marine->pos, attack_target) > 5.0f) {
            Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, attack_target);
        }
    }

    for (const auto& tank : siege_tanks) {
        if (tank->orders.empty() && Distance2D(tank->pos, attack_target) > 5.0f) {
            Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, attack_target);
        }
    }

    if (!is_attacking) {
        is_attacking = true;
    }
}
