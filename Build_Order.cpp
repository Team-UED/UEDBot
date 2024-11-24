#include "BasicSc2Bot.h"

// Replace Sleep(10) with the following code
using namespace sc2;


void BasicSc2Bot::ExecuteBuildOrder() {
	BuildBarracks();
	BuildFactory();
	BuildEngineeringBay();
	BuildOrbitalCommand();
	BuildTechLabAddon();
	BuildStarport();

	// checking if swapping is in progress
	Swap(swap_a, swap_b, false);
	BuildFusionCore();
	BuildArmory();
}

// Build Barracks if we have a Supply Depot and enough resources
void BasicSc2Bot::BuildBarracks() {
	const ObservationInterface* observation = Observation();

	// Get Supply Depots
	Units supply = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));

	// Can't built Barracks without Supply Depots
	if (supply.empty()) {
		return;
	}

	// Build only 1 Barrack
	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	if (barracks.empty() && observation->GetMinerals() >= 150) {
		TryBuildStructure(ABILITY_ID::BUILD_BARRACKS, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Engineering bay if we have a Barrack and enough resources
void BasicSc2Bot::BuildEngineeringBay() {
	const ObservationInterface* observation = Observation();

	// Get Barracks
	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	// Get Startports
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

	// Can't build Engineering bay without Barracks
	if (barracks.empty() || !first_battlecruiser) {
		return;
	}

	// Build only 1 engineering bay (After Starports)
	Units engineeringbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
	if (engineeringbays.empty() && !starports.empty() && observation->GetMinerals() >= 125) {
		TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Orbital Command if we have a Command Center and enough resources
void BasicSc2Bot::BuildOrbitalCommand() {
	const ObservationInterface* observation = Observation();

	// Get Barracks and Factories
	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	// Can't build Orbital Command without Barracks or Factories
	if (barracks.empty() || factories.empty()) {
		return;
	}

	// Find a Command Center that can be upgraded
	Units command_centers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));

	if (command_centers.empty()) {
		return;
	}

	for (const auto& command_center : command_centers) {
		// Check if this Command Center is already being upgraded
		bool is_upgrading = false;
		for (const auto& order : command_center->orders) {
			if (order.ability_id == ABILITY_ID::MORPH_ORBITALCOMMAND) {
				is_upgrading = true;
				break;
			}
		}
		// If its not upgrading, upgrade it
		if (!is_upgrading && observation->GetMinerals() >= 150) {
			Actions()->UnitCommand(command_center, ABILITY_ID::MORPH_ORBITALCOMMAND);
			return;
		}
	}
}

// Build Factory if we have a Barracks and enough resources
void BasicSc2Bot::BuildFactory() {
	const ObservationInterface* observation = Observation();

	// Get Barracks
	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));

	// Can't build Factory without Barracks
	if (barracks.empty()) {
		return;
	}

	// Build only 1 Factory
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units factories_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYFLYING));
	if (factories_f.empty() && factories.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_FACTORY, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Starport if we have a Factory and enough resources
void BasicSc2Bot::BuildStarport() {
	const ObservationInterface* observation = Observation();

	// Get Factories
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	// Can't build Starports without Factories
	if (factories.empty()) {
		return;
	}

	// Build only 1 Starport
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units starports_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTFLYING));
	if (starports_f.empty() && starports.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {

		std::cout << "Building Starport" << std::endl;
		TryBuildStructure(ABILITY_ID::BUILD_STARPORT, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build Tech lab if we have a Factory and enough resources
void BasicSc2Bot::BuildTechLabAddon() {
	const ObservationInterface* obs = Observation();

	// Get Barracks
	Units barracks = obs->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::TERRAN_BARRACKS && !unit.is_flying &&
			!unit.add_on_tag;
		});

	// Get Factories
	Units factories = obs->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::TERRAN_FACTORY && !unit.is_flying &&
			!unit.add_on_tag;
		});

	// Get Starports
	Units starports = obs->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::TERRAN_STARPORT && !unit.is_flying &&
			!unit.add_on_tag;
		});

	// use 3 bits to represent buildings without any addons
	char addon_bits = 0;
	addon_bits |= (!barracks.empty() ? 1 : 0);
	addon_bits |= (!factories.empty() ? 2 : 0);
	addon_bits |= (!starports.empty() ? 4 : 0);

	// any is not empty then..
	if (addon_bits != 0) {

		// enough resources
		if (obs->GetMinerals() >= 50 && obs->GetVespene() >= 25)
		{
			for (size_t i = 0; i < 3; ++i)
			{
				if (addon_bits & 1)
				{
					// Get the first building without an addon
					const Unit* building = nullptr;
					switch (i)
					{
					case 0:
						building = barracks.front();
						break;
					case 1:
						building = factories.front();
						break;
					case 2:
						building = starports.front();
						break;
					}
					// Build a Tech Lab
					Actions()->UnitCommand(building, ABILITY_ID::BUILD_TECHLAB);
					break;
				}
				addon_bits >>= 1;
			}
		}
	}
	return;
}

// Build Fusion Core if we have a Starport and enough resources
void BasicSc2Bot::BuildFusionCore() {
	const ObservationInterface* observation = Observation();

	// Get Starports
	Units starports = observation->GetUnits(Unit::Alliance::Self, [](const Unit& unit)
		{
			return unit.unit_type == UNIT_TYPEID::TERRAN_STARPORT && unit.tag != 0;
		});


	// Can't build fusion core without Starports
	if (starports.empty()) {
		return;
	}

	// Build only 1 Fusion core
	Units fusioncore = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));
	if (fusioncore.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 150)) {

		std::cout << "Building Fusion Core" << std::endl;
		TryBuildStructure(ABILITY_ID::BUILD_FUSIONCORE, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Armory if we have a Fusion core and enough resources
void BasicSc2Bot::BuildArmory() {
	const ObservationInterface* observation = Observation();

	// Can't build Armory core without the First Battlecruiser
	if (!first_battlecruiser) {
		return;
	}

	// Build only 1 Armory
	Units armories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ARMORY));
	if (armories.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 50)) {
		TryBuildStructure(ABILITY_ID::BUILD_ARMORY, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Swaps a Factory with a Starport
void BasicSc2Bot::Swap(const Unit* a, const Unit* b, bool lift) {

	// lift buildings
	if (lift) {
		swap_in_progress = true;
		Actions()->UnitCommand(a, ABILITY_ID::LIFT);
		Actions()->UnitCommand(b, ABILITY_ID::LIFT);
		swap_a = a;
		swap_b = b;
	}
	else
	{
		if (swap_in_progress && a->is_flying && b->is_flying)
		{
			const ObservationInterface* obs = Observation();
			Point2D swap_position_a = a->pos;
			Point2D swap_position_b = b->pos;
			Actions()->UnitCommand(a, ABILITY_ID::LAND, swap_position_b);
			Actions()->UnitCommand(b, ABILITY_ID::LAND, swap_position_a);
			swap_in_progress = false;
		}
	}
	return;
}

