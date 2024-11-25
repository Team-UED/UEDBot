
#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Battlecruisers
void BasicSc2Bot::ControlBattlecruisers() {
	Jump();
	TargetBattlecruisers();
	RetreatCheck();
}

// Use Tactical Jump into enemy base
void BasicSc2Bot::Jump() {

	// Check if the main base is under attack; don't use Tactical Jump in that case
	if (IsMainBaseUnderAttack()) {
		return;
	}

	// Check if any Battlecruiser is still retreating
	for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
		if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && battlecruiser_retreating[unit]) {
			// Wait until all retreating Battlecruisers finish their retreat
			return;
		}
	}

	// No retreating Battlecruisers, proceed with Tactical Jump logic
	for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
		// Check if the unit is a Battlecruiser with full health and not retreating
		if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER &&
			unit->health >= unit->health_max &&
			Distance2D(unit->pos, enemy_start_location) > 40.0f &&
			HasAbility(unit, ABILITY_ID::EFFECT_TACTICALJUMP)) {
			Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_TACTICALJUMP, enemy_start_location);
		}
	}
}


/// Target enemy units based on threat levels
void BasicSc2Bot::TargetBattlecruisers() {

	// Maximum distance to consider for targetting
	const float max_distace_for_target = 40.0f;

	// Get Battlecruisers
	const Units battlecruisers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER));

	// Exit when there are no battlecruisers
	if (battlecruisers.empty()) {
		return;
	}

	// Number of Battlecruisers in combat
	int num_battlecruisers_in_combat = UnitsInCombat(UNIT_TYPEID::TERRAN_BATTLECRUISER);

	// Threshold for "kiting" behavior
	const int threat_threshold = 10 * num_battlecruisers_in_combat;

    for (const auto& battlecruiser : battlecruisers) {

		// Disables targetting while Jumping
		if (!battlecruiser->orders.empty()) {
			const AbilityID current_ability = battlecruiser->orders.front().ability_id;
			if (current_ability == ABILITY_ID::EFFECT_TACTICALJUMP) {
				continue;
			}
		}

        int total_threat = CalculateThreatLevel(battlecruiser);

		// Determine whether to retreat based on the threat level
		// retreat if the total threat level is above the threshold
		if (total_threat >= threat_threshold && (total_threat != 0) && (threat_threshold != 0)) {

			float health_threshold = 0.0f;

			// Extremly Strong anti air units -> Retreat when hp = 400
			if (total_threat >= 2.0 * threat_threshold) {
				health_threshold = 450.0f;
			}
			// Strong anti air units -> Retreat when hp = 400
			else if (total_threat >= 1.5 * threat_threshold && total_threat <= 2.0 * threat_threshold) {
				health_threshold = 350.0f;
			}
			// Moderate anti air units -> Retreat when hp = 200
			else if (total_threat > threat_threshold && total_threat <= 1.5 * threat_threshold) {
				health_threshold = 250.0f;
			}

            if (battlecruiser->health <= health_threshold) {
                Retreat(battlecruiser);
            }
            else {
                const Unit* target = GetClosestThreat(battlecruiser);
                // Kite enemy units
                if (target) {
                    // Skip kiting if the target is too far away
                    if (Distance2D(battlecruiser->pos, target->pos) > 12.0f) {
                        continue;
                    }
                    else {
                        Actions()->UnitCommand(battlecruiser, ABILITY_ID::MOVE_MOVE, GetKiteVector(battlecruiser, target));
                    }
                }
            }
        }
        // Do not kite if the total threat level is below the threshold
        else {
			// Retreat Immediately if the Battlecruiser is below 150 health
			if ((battlecruiser->health <= 150.0f)) {
				Retreat(battlecruiser);
				return;
			}

            const Unit* target = nullptr;
            float min_distance = std::numeric_limits<float>::max();
            float min_hp = std::numeric_limits<float>::max();

            auto UpdateTarget = [&](const Unit* enemy_unit, float max_distance) {
                float distance = Distance2D(battlecruiser->pos, enemy_unit->pos);
                if (!enemy_unit->is_alive || distance > max_distance) {
                    return;
                }
                if (distance < min_distance || (distance == min_distance && enemy_unit->health < min_hp)) {
                    min_distance = distance;
                    min_hp = enemy_unit->health;
                    target = enemy_unit;
                }
            };

            // Count turrets
            int num_turrets = 0;
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                if (std::find(turret_types.begin(), turret_types.end(), enemy_unit->unit_type) != turret_types.end()) {
                    num_turrets++;
                }
            }

            // Prioritize targets based on rules
            auto PrioritizeTargets = [&](const std::vector<UNIT_TYPEID>& types, float max_distance) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    if (std::find(types.begin(), types.end(), enemy_unit->unit_type) != types.end()) {
                        UpdateTarget(enemy_unit, max_distance);
                    }
                }
            };

            // 1st Priority: Enemy units (excluding turrets based on turret conditions)
            for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                auto threat = threat_levels.find(enemy_unit->unit_type);
                if (threat != threat_levels.end()) {
                    if (std::find(turret_types.begin(), turret_types.end(), enemy_unit->unit_type) != turret_types.end()) {
                        // Avoid turrets when conditions apply
                        if (num_turrets > 2 * num_battlecruisers_in_combat || total_threat - (3 * num_turrets) != 0) {
                            continue;
                        }
                    }
                    UpdateTarget(enemy_unit, max_distace_for_target);
                }
            }

            if (!target) {
                PrioritizeTargets(worker_types, max_distace_for_target);
            }

            // 3rd Priority -> Turrets
            if (!target) {
                PrioritizeTargets(turret_types, max_distace_for_target);
            }

            // 4th Priority -> Any units that are not structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    if (!std::any_of(unit_type_data.attributes.begin(), unit_type_data.attributes.end(), [](Attribute attr) { return attr == Attribute::Structure; }) &&
                        enemy_unit->unit_type != UNIT_TYPEID::ZERG_LARVA &&
                        enemy_unit->unit_type != UNIT_TYPEID::ZERG_EGG) {
                        UpdateTarget(enemy_unit, max_distace_for_target);
                    }
                }
            }

            // 5th Priority -> Supply structures
            if (!target) {
                PrioritizeTargets(resource_units, max_distace_for_target);
            }

            // 6th Priority -> Any structures
            if (!target) {
                for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
                    const UnitTypeData& unit_type_data = Observation()->GetUnitTypeData().at(enemy_unit->unit_type);
                    if (std::any_of(unit_type_data.attributes.begin(), unit_type_data.attributes.end(), [](Attribute attr) { return attr == Attribute::Structure; })) {
                        UpdateTarget(enemy_unit, max_distace_for_target);
                    }
                }
            }

            // Attack the selected target
            if (target && target->NotCloaked) {
				// No turret nearby or turret is the target or there are only turrets in threat radius -> Attack
                Actions()->UnitCommand(battlecruiser, ABILITY_ID::ATTACK_ATTACK, target);
}
            // No target
            else {
                if (battlecruiser->health >= 500.0f) {
                    if (Distance2D(battlecruiser->pos, enemy_start_location) < 40.0f) {
                        Retreat(battlecruiser);
                        return;
                    }
                    else {
                        // Check if any Battlecruiser is still retreating
                        for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
                            if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && battlecruiser_retreating[unit]) {
                                // Wait until all retreating Battlecruisers finish their retreat
                                return;
                            }
                        }
                        Actions()->UnitCommand(battlecruiser, ABILITY_ID::EFFECT_TACTICALJUMP, enemy_start_location);
                    }
                }
				else {
					Retreat(battlecruiser);
					return;
				}
			}
		}
	}
}

void BasicSc2Bot::Retreat(const Unit* unit) {

	if (unit == nullptr) {
		return;
	}

	battlecruiser_retreat_location[unit] = retreat_location;
	battlecruiser_retreating[unit] = true;
	Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, retreat_location);
}

void BasicSc2Bot::RetreatCheck() {
	// Distance threshold for arrival
	const float arrival_threshold = 5.0f;

	for (const auto& battlecruiser : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BATTLECRUISER))) {
		// Check if the Battlecruiser has reached the retreat location
        if (battlecruiser_retreating[battlecruiser] &&
            Distance2D(battlecruiser->pos, retreat_location) <= arrival_threshold &&
            battlecruiser->health >= 550.0f) {
            battlecruiser_retreat_location.erase(battlecruiser);
            battlecruiser_retreating[battlecruiser] = false;
        }
    }
}

