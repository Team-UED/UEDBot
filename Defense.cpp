#include "BasicSc2Bot.h"

void BasicSc2Bot::Defense() {
	//BlockRamp();
	EarlyDefense();

	if (current_gameloop % 42 == 0)
	{
		LateDefense();
	}
}

void BasicSc2Bot::EarlyDefense() {
	if (!IsAnyBaseUnderAttack()) {
		return;
	}
	// Get all our bases
	Units our_bases = Observation()->GetUnits(Unit::Alliance::Self, IsTownHall());

	// Collect enemy units near our bases
	Units enemy_units;
	for (const auto& base : our_bases) {
		Units enemies_near_base = Observation()->GetUnits(Unit::Alliance::Enemy, [base](const Unit& unit) {
			return Distance2D(unit.pos, base->pos) < 20.0f &&
				unit.display_type == Unit::DisplayType::Visible;
			});
		enemy_units.insert(enemy_units.end(), enemies_near_base.begin(), enemies_near_base.end());
	}

	if (enemy_units.empty()) {
		return;
	}

	// Find the highest threat enemy unit
	const Unit* primary_target = nullptr;
	for (const auto& enemy : enemy_units) {
		if (!primary_target || enemy->weapon_cooldown < primary_target->weapon_cooldown) {
			primary_target = enemy;
		}
	}

	// Get and manage our combat units
	Units marines = Observation()->GetUnits(Unit::Alliance::Self,
		IsUnit(UNIT_TYPEID::TERRAN_MARINE));
	Units siege_tanks = Observation()->GetUnits(
		Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

	// Command marines
	for (const auto& marine : marines) {
		if (marine->orders.empty() ||
			marine->orders.front().ability_id != ABILITY_ID::ATTACK) {
			Actions()->UnitCommand(marine, ABILITY_ID::ATTACK,
				primary_target ? primary_target->pos
				: enemy_units.front()->pos);
		}
	}

	// Command siege tanks
	for (const auto& tank : siege_tanks) {
		if (tank->orders.empty() ||
			tank->orders.front().ability_id != ABILITY_ID::ATTACK) {
			Actions()->UnitCommand(tank, ABILITY_ID::ATTACK,
				primary_target ? primary_target->pos
				: enemy_units.front()->pos);
		}
	}
}

// Builds additional defense structures like Missile Turrets.
void BasicSc2Bot::LateDefense() {
	// Check if we have an Engineering Bay (required for Missile Turrets)
	Units engineering_bays = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
	if (engineering_bays.empty()) {
		return; // Can't build Missile Turrets without an Engineering Bay
	}

	// Build Missile Turrets near bases and important locations
	Units missile_turrets = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MISSILETURRET));
	// Limit the number of Missile Turrets
	//TODO: change
	/*const int desired_turrets = 2 * bases.size();
	if (missile_turrets.size() >= desired_turrets || !first_battlecruiser) {
		return;
	}*/

	std::vector<Point2D> turret_locations;
	std::vector<Point2D> mineral_convexhull = main_mineral_convexHull;
	turret_locations.reserve(2);

	for (const auto& base : bases) {
		// Build a Missile Turret near the base
		//Point2D turret_position = base->pos + Point2D(5.0f, 0.0f); // Adjust as needed
		Point2D base_pos = base->pos;
		if (base_pos != start_location) {
			mineral_convexhull = get_close_mineral_points(base_pos);
		}
		turret_locations = find_terret_location_btw(mineral_convexhull, base_pos);

		for (const auto& t : turret_locations)
		{
			if (TryBuildStructureAtLocation(ABILITY_ID::BUILD_MISSILETURRET, UNIT_TYPEID::TERRAN_SCV, t)) {
				return; // Build one turret at a time
			}
		}
	}
}