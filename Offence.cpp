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

    // Search for remaining enemy structures
    Units enemy_structures = observation->GetUnits(Unit::Alliance::Enemy, [this](const Unit& unit) {
        const UnitTypeData& type_data = Observation()->GetUnitTypeData()[unit.unit_type];
        return std::find(type_data.attributes.begin(), type_data.attributes.end(), Attribute::Structure) != type_data.attributes.end();
    });

    // If enemy structures found, set closest one as new attack target
    Point2D attack_target = enemy_start_location;
    if (!enemy_structures.empty()) {
        // Find closest enemy structure to our army's center
        Point2D army_center = Point2D(0, 0);
        int combat_unit_count = 0;
        
        // Calculate army center
        for (const auto& marine : marines) {
            army_center += marine->pos;
            combat_unit_count++;
        }
        for (const auto& tank : siege_tanks) {
            army_center += tank->pos;
            combat_unit_count++;
        }
        for (const auto& bc : battlecruisers) {
            army_center += bc->pos;
            combat_unit_count++;
        }
        
        if (combat_unit_count > 0) {
            army_center /= combat_unit_count;
        }

        // Find closest enemy structure
        float min_distance = std::numeric_limits<float>::max();
        for (const auto& structure : enemy_structures) {
            float distance = Distance2D(army_center, structure->pos);
            if (distance < min_distance) {
                min_distance = distance;
                attack_target = structure->pos;
            }
        }
    } else {
        // If no enemy structures found, search unexplored areas
        const GameInfo& game_info = observation->GetGameInfo();
        std::vector<Point2D> potential_locations;

        // Generate grid of potential base locations to check
        float step_size = 20.0f;
        for (float x = 0; x < game_info.width; x += step_size) {
            for (float y = 0; y < game_info.height; y += step_size) {
                Point2D location(x, y);
                if (observation->IsPathable(location)) {
                    potential_locations.push_back(location);
                }
            }
        }

        // Find unexplored potential location closest to army
        Point2D army_center = Point2D(0, 0);
        int combat_unit_count = marines.size() + siege_tanks.size() + battlecruisers.size();
        
        if (combat_unit_count > 0) {
            for (const auto& marine : marines) army_center += marine->pos;
            for (const auto& tank : siege_tanks) army_center += tank->pos;
            for (const auto& bc : battlecruisers) army_center += bc->pos;
            army_center /= combat_unit_count;

            float min_distance = std::numeric_limits<float>::max();
            for (const auto& loc : potential_locations) {
                if (observation->GetVisibility(loc) == sc2::Visibility::Hidden) {  // Only consider unexplored locations
                    float distance = Distance2D(army_center, loc);
                    if (distance < min_distance) {
                        min_distance = distance;
                        attack_target = loc;
                    }
                }
            }
        }
    }

    // Command units to attack the target
    if (!marines.empty() || !siege_tanks.empty() || !battlecruisers.empty()) {
        // Split army into smaller groups for better coverage
        int group_size = 5;
        std::vector<Point2D> attack_positions;
        
        // Generate positions around the main target
        for (int i = 0; i < 4; i++) {
            float angle = i * 3.14159f / 2;  // Split into 4 directions
            float offset = 10.0f;  // Distance from main target
            Point2D offset_pos(attack_target.x + std::cos(angle) * offset,
                             attack_target.y + std::sin(angle) * offset);
            attack_positions.push_back(offset_pos);
        }
        attack_positions.push_back(attack_target);  // Add main target position

        // Assign units to different attack positions
        int current_group = 0;
        for (const auto& marine : marines) {
            Actions()->UnitCommand(marine, ABILITY_ID::ATTACK, 
                                 attack_positions[current_group % attack_positions.size()]);
            if (++current_group % group_size == 0) current_group++;
        }

        for (const auto& tank : siege_tanks) {
            Actions()->UnitCommand(tank, ABILITY_ID::ATTACK,
                                 attack_positions[current_group % attack_positions.size()]);
            if (++current_group % group_size == 0) current_group++;
        }

        for (const auto& bc : battlecruisers) {
            Actions()->UnitCommand(bc, ABILITY_ID::ATTACK,
                                 attack_positions[current_group % attack_positions.size()]);
            if (++current_group % group_size == 0) current_group++;
        }
    }

    // Keep producing units during the attack
    if (!is_attacking) {
        is_attacking = true;
    }
}