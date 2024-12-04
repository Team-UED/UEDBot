#include "BasicSc2Bot.h"

void BasicSc2Bot::Offense() {

    const ObservationInterface* observation = Observation();
    Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
	Units battlecruisers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));
    Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

   // Check if we should start attacking
    if (!is_attacking) {

        if (starports.empty()) {
            return;
        }

		const Unit* starport = starports.front();

		// Default timing for first attack When battlecruiser production is at 40%
        float timing = 0.4f;
        const GameInfo& game_info = observation->GetGameInfo();

		if (game_info.map_name == "Cactus Valley LE (Void)") {
            if ((nearest_corner_enemy.x == nearest_corner_ally.x) ||
                (nearest_corner_enemy.y == nearest_corner_ally.y)) {
                timing = 0.55f;
            }
			else {
				timing = 0.4f;
			}
		}
        else {
            timing = 0.5f;
        }

		// Check if we have enough army to attack
        // At least one battlecruisers is currently in combat and not retreating
        if (UnitsInCombat((UNIT_TYPEID::TERRAN_BATTLECRUISER)) > 0) {
			bool not_retreating = false;
            for (const auto& battlecruiser :battlecruisers) {
                if (!battlecruiser_retreating[battlecruiser]) {
					not_retreating = true;
					break;
                }
            }
            if (EnoughArmy() && not_retreating) {
                if (need_clean_up) {
                    CleanUp();
                }
                else {
                    // Continue attacking
                    AllOutRush();
                }
                return;
            }
        }
        // No Battlecruisers in combat
        else {
			// No Battlecruisers trained yet, or all destroyed
            if (num_battlecruisers == 0) {
                if (!starport->orders.empty()) {
                    for (const auto& order : starport->orders) {
                        if (order.ability_id == ABILITY_ID::TRAIN_BATTLECRUISER) {
                            if (order.progress >= timing - 0.02f && order.progress <= timing + 0.02f) {
                                if (need_clean_up) {
                                    CleanUp();
                                } else {
                                    // Continue attacking
                                    AllOutRush();
                                }
                            }
                            return;
                        }
                    }
                }
            }
            // Battlecruisers are not in combat, and retreating
            else {
                bool attack = true;
                for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
                    if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && battlecruiser_retreating[unit]) {
                        if (unit->health < 0.4f * unit->health_max) {
                            attack = false;
                            break;
                        }
                    }
                }
                // If all retreating Battlecruisers are healthy, execute the attack
                if (attack) {
                    if (need_clean_up) {
                        CleanUp();
                    } else {
                        // Continue attacking
                        AllOutRush();
                    }
                }
            }
        }
    }
    else {
		ContinuousMove();
        // Check if our army is mostly dead
        Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
        // If army is severely depleted, retreat and rebuild before attacking again
        if (AllRetreating()) {
            is_attacking = false;
            for (const auto& marine : marines) {
                if (unit_attacking[marine]) {
					unit_attacking[marine] = false;
                }
                Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, rally_barrack);
            }
            for (const auto& tank : siege_tanks) {
                if (unit_attacking[tank]) {
                    unit_attacking[tank] = false;
                }
                Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, rally_factory);
            }
        }
        else {
            if (need_clean_up) {
                CleanUp();
            } else {
                if (EnoughArmy()) {
                    AllOutRush();
                }
            }
        }
    }
}

void BasicSc2Bot::AllOutRush() {
    const ObservationInterface* observation = Observation();

    // Get all our combat units
    Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
	Units battlecruisers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

	// Check if we have any units to attack with
	if (marines.empty() && siege_tanks.empty()) {
		return;
	}

    Units marine_near_rally;
	Units tank_near_rally;

	for (const auto& marine : marines) {
		if (Distance2D(marine->pos, rally_barrack) < 5.0f) {
			marine_near_rally.push_back(marine);
		}
	}
	for (const auto& tank : siege_tanks) {
		if (Distance2D(tank->pos, rally_factory) < 5.0f) {
			tank_near_rally.push_back(tank);
		}
	}

    // Determine attack target 
    attack_target = enemy_start_location;

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

		// Search for any visible unit left on the map
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
		// No visible units left, search for snapshot units
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
                need_clean_up = true;
                return;
            }
        }
    }

    // Move units to the target location
    for (const auto& marine : marine_near_rally) {
        if (marine->orders.empty() && Distance2D(marine->pos, attack_target) > 5.0f) {
            unit_attacking[marine] = true;
            Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, attack_target);
        }
    }

    for (const auto& tank : tank_near_rally) {
        if (tank->orders.empty() && Distance2D(tank->pos, attack_target) > 5.0f) {
			unit_attacking[tank] = true;
            Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, attack_target);
        }
    }

    if (enemy_base_destroyed && !battlecruisers.empty()) {
        for (const auto& battlecruiser : battlecruisers) {
            if (battlecruiser->orders.empty() && Distance2D(battlecruiser->pos, attack_target) > 5.0f) {
                Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, attack_target);
            }
        }
    }

    if (!is_attacking) {
        is_attacking = true;
    }
}


void BasicSc2Bot::CleanUp() {
    const ObservationInterface *observation = Observation();

    // Get all our combat units
    Units marines = observation->GetUnits(Unit::Alliance::Self,
                                          IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    Units siege_tanks = observation->GetUnits(
        Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

    Point2D attack_target = enemy_start_location;
    // sort scout locations by distance to the start location
    std::sort(scout_points.begin(), scout_points.end(),
              [this](const Point2D &a, const Point2D &b) {
                  return Distance2D(a, enemy_start_location) <
                         Distance2D(b, enemy_start_location);
              });

    // set the attack target
    if (clean_up_index >= scout_points.size()) {
        clean_up_index = 0;
    }
    attack_target = scout_points[clean_up_index];

    // Check for enemy units or structures near the attack target, including
    // snapshots
    for (const auto &enemy_unit :
         observation->GetUnits(Unit::Alliance::Enemy)) {
        if ((enemy_unit->display_type == Unit::DisplayType::Visible ||
             enemy_unit->display_type == Unit::DisplayType::Snapshot) &&
            enemy_unit->is_alive &&
            Distance2D(enemy_unit->pos, attack_target) < 25.0f) {
            need_clean_up = false;
            break;
        }
    }

    // Move units to the target location
    for (const auto &marine : marines) {
        if (marine->orders.empty() &&
            Distance2D(marine->pos, attack_target) > 5.0f) {
            Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE,
                                   attack_target);

            if (marine->orders.empty() ||
                sc2::Distance2D(marine->pos, attack_target) <= 5.0f) {
                clean_up_index++;
            }
        }
    }
}

bool BasicSc2Bot::EnoughArmy() {
	const ObservationInterface* observation = Observation();

	Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
	Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

	if (marines.empty() && siege_tanks.empty()) {
		return false;
	}

    int marine_count = 0;
	int tank_count = 0;

    if (!marines.empty()) {
        for (const auto& marine : marines) {
            if (Distance2D(marine->pos, rally_barrack) <= 5.0f) {
                marine_count++;
            }
        }
    }

    if (!siege_tanks.empty()) {
        for (const auto& tank : siege_tanks) {
            if (Distance2D(tank->pos, rally_factory) <= 5.0f) {
                tank_count++;
            }
        }
    }

    if (marine_count + tank_count >= 9) {
        return true;
    }
}

void BasicSc2Bot::ContinuousMove() {
    const ObservationInterface* observation = Observation();

    Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

    if (marines.empty() && siege_tanks.empty()) {
        return;
    }

    if (!marines.empty()) {
        for (const auto& marine : marines) {
            if (unit_attacking[marine] && marine->orders.empty()) {
				Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, attack_target);
            }
        }
    }

	if (!siege_tanks.empty()) {
		for (const auto& tank : siege_tanks) {
			if (unit_attacking[tank] && tank->orders.empty()) {
				Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, attack_target);
			}
		}
	}
}

bool BasicSc2Bot::AllRetreating() {
    const ObservationInterface* observation = Observation();

    Units battlecruisers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

	bool retreat = true;

	// All Battlecruisers destroyed, or none trained yet
    if (battlecruisers.empty()) {
        if (!first_battlecruiser_trained) {
            retreat = false;
        }
        else {
            return retreat;
        }
    }

	for (const auto& battlecruiser : battlecruisers) {
		if (!battlecruiser_retreating[battlecruiser] || battlecruiser->health > 150.0f) {
			retreat = false;
		}
	}

	return retreat;
}