#include "BasicSc2Bot.h"

void BasicSc2Bot::Offense() {

    const ObservationInterface* observation = Observation();
    if (num_marines >= 1) {
        CleanUp();
    }

  //  // Check if we should start attacking
  //  if (!is_attacking) {

  //      Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
  //      
  //      if (starports.empty()) {
  //          return;
  //      }

		//const Unit* starport = starports.front();

		//// Default timing for first attack When battlecruiser production is at 40%
  //      float timing = 0.4f;

		//// Attack timing for the case where the enemy is not in diagonal opposite (Production at 60%)
  //      if ((nearest_corner_enemy.x == nearest_corner_ally.x) ||
  //          (nearest_corner_enemy.y == nearest_corner_ally.y)) {
  //          timing = 0.60f;
  //      }

		//// Check if we have enough army to attack
  //      // At least one battlecruisers is currently in combat and not retreating
  //      if (UnitsInCombat((UNIT_TYPEID::TERRAN_BATTLECRUISER)) > 0) {
  //          for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
  //              if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && !battlecruiser_retreating[unit]) {
  //                  if (num_marines >= 8 - UnitsInCombat(UNIT_TYPEID::TERRAN_MARINE) && 
  //                      num_siege_tanks >= 1 - UnitsInCombat(UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)) {
  //                      if (need_clean_up) {
  //                          CleanUp();
  //                      } else {
  //                          // Continue attacking
  //                          AllOutRush();
  //                      }
  //                      return;
  //                  }
  //              }
  //          }
  //      }
  //      // No Battlecruisers in combat
  //      else {
		//	// No Battlecruisers trained yet, or all destroyed
  //          if (num_battlecruisers == 0) {
  //              if (!starport->orders.empty()) {
  //                  for (const auto& order : starport->orders) {
  //                      if (order.ability_id == ABILITY_ID::TRAIN_BATTLECRUISER) {
  //                          if (order.progress >= timing) {
  //                              if (need_clean_up) {
  //                                  CleanUp();
  //                              } else {
  //                                  // Continue attacking
  //                                  AllOutRush();
  //                              }
  //                          }
  //                          return;
  //                      }
  //                  }
  //              }
  //          }
  //          // Battlecruisers are not in combat, and retreating
  //          else {
  //              bool attack = true;
  //              for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
  //                  if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && battlecruiser_retreating[unit]) {
  //                      if (unit->health < 0.4f * unit->health_max) {
  //                          attack = false;
  //                          break;
  //                      }
  //                  }
  //              }
  //              // If all retreating Battlecruisers are healthy, execute the attack
  //              if (attack) {
  //                  if (need_clean_up) {
  //                      CleanUp();
  //                  } else {
  //                      // Continue attacking
  //                      AllOutRush();
  //                  }
  //              }
  //          }
  //      }
    }
    else {
        // Check if our army is mostly dead
        Units marines = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		Units siege_tanks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
        // If army is severely depleted, retreat and rebuild before attacking again
        if (num_marines < 5) {
            is_attacking = false;
            for (const auto& marine : marines) {
                Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, rally_barrack);
            }
            for (const auto& tank : siege_tanks) {
                Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, rally_factory);
            }
        }
        else {
            if (need_clean_up) {
                CleanUp();
            } else {
                // Continue attacking
                AllOutRush();
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

    // Determine attack target 
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


bool BasicSc2Bot::CleanUp() {
    Point2D attack_target = enemy_start_location;
    // sort scout locations by distance to the start location
    std::sort(scout_points.begin(), scout_points.end(),
              [this](const Point2D &a, const Point2D &b) {
                  return Distance2D(a, enemy_start_location) <
                         Distance2D(b, enemy_start_location);
              });

    // set the attack target
    if (current_scout_index >= scout_points.size()) {
        current_scout_index = 0;
    }
    attack_target = scout_points[current_scout_index];

    // Move units to the target location
    for (const auto &marine : marines) {
        if (marine->orders.empty() &&
            Distance2D(marine->pos, attack_target) > 5.0f) {
            Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE,
                                   attack_target);

            if (marine->orders.empty() ||
                sc2::Distance2D(marine->pos, current_target) <= 5.0f) {
                current_scout_index++;
            }
        }
    }
    for (const auto &tank : siege_tanks) {
        if (tank->orders.empty() &&
            Distance2D(tank->pos, attack_target) > 5.0f) {
            Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, attack_target);
            if (tank->orders.empty() ||
                sc2::Distance2D(tank->pos, current_target) <= 5.0f) {
                current_scout_index++;
            }
        }
    }