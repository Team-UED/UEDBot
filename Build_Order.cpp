#include "BasicSc2Bot.h"

using namespace sc2;


void BasicSc2Bot::ExecuteBuildOrder() {
	BuildBarracks();
	BuildFactory();
	BuildEngineeringBay();
	BuildOrbitalCommand();
	BuildTechLabAddon();
	BuildStarport();
	Swap();
	BuildFusionCore();
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
	if (barracks.empty()) {
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

	// Get Engineering bays
	Units engineeringbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
	// Get Fusion core
	Units fusioncore = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));
	// Can't build Orbital Command without Engineering bay
	// Set Fusioncore as prerequisite for resource management

	if (engineeringbays.empty() && fusioncore.empty()) {
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
		TryBuildStructure(ABILITY_ID::BUILD_STARPORT, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build Tech lab if we have a Factory and enough resources
void BasicSc2Bot::BuildTechLabAddon() {
	const ObservationInterface* observation = Observation();

	// Get Factories
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units factories_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYFLYING));

	// Can't build Tech Lab without Factories
	if (factories.empty() && factories_f.empty()) {
		return;
	}

	// Get Starports
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	bool starport_has_addon = false;

	if (!starports.empty()) {
		for (const auto& starport : starports) {
			if (starport->add_on_tag != 0) {
				starport_has_addon = true;
				break;
			}
		}
	}

	// Get Techlabs
	Units techlabs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_TECHLAB));

	// Get Fusion cores
	Units fusioncores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));


	// Build Tech Lab if we have a factory and enough resources
	if (observation->GetMinerals() >= 50 && observation->GetVespene() >= 25 && !techlab_building_in_progress) {
		{
			// Find a suitable Factory to build Tech Lab
			current_factory = nullptr;

			// Factory without an addon
			if (!factories.empty()) {
				for (const auto& fc : factories) {
					if (fc->unit_type == UNIT_TYPEID::TERRAN_FACTORY &&
						fc->add_on_tag == 0 &&
						fc->build_progress == 1.0f &&
						fc->orders.empty()) {
						current_factory = fc;
						break;
					}
				}
			}

			// If no suitable landed Factory found, check for flying Factories
			if (!current_factory && !factories_f.empty()) {
				current_factory = factories_f.front();
			}


			// If a suitable Factory is found, build Tech Lab
			if (current_factory) {
				// If the Factory is landed, attempt to build Tech Lab
				// Lift the Factory if it failed (orders.empty())
				if (current_factory->unit_type == UNIT_TYPEID::TERRAN_FACTORY) {
					if (!starport_has_addon) {
						Actions()->UnitCommand(current_factory, ABILITY_ID::BUILD_TECHLAB_FACTORY);
						if (current_factory->orders.empty()) {
							Actions()->UnitCommand(current_factory, ABILITY_ID::LIFT);
						}
					}
					else {
						if (!fusioncores.empty()) {
							Actions()->UnitCommand(current_factory, ABILITY_ID::BUILD_TECHLAB_FACTORY);
							if (current_factory->orders.empty()) {
								Actions()->UnitCommand(current_factory, ABILITY_ID::LIFT);
							}
						}
					}
				}
				// If the Factory is flying, find a suitable landing spot
				else if (current_factory->unit_type == UNIT_TYPEID::TERRAN_FACTORYFLYING) {
					// Build first Tech Lab right away, Delay the second Tech Lab until We have a Fusion core
					if (techlabs.empty() || (!techlabs.empty() && starport_has_addon && !fusioncores.empty())) {
						// Generate a random nearby location
						float rx = GetRandomScalar();
						float ry = GetRandomScalar();
						Point2D build_location(current_factory->pos.x + rx * 15.0f, current_factory->pos.y + ry * 15.0f);
						Actions()->UnitCommand(current_factory, ABILITY_ID::BUILD_TECHLAB_FACTORY, build_location);
						techlab_building_in_progress = true;
					}
				}
			}
		}

	}
	// Reset pointer and flag 
	if (techlab_building_in_progress && current_factory) {
		if (current_factory->orders.empty() || (current_factory->add_on_tag != 0)) {
			techlab_building_in_progress = false;
			current_factory = nullptr;
		}
	}
}

// Build Fusion Core if we have a Starport and enough resources
void BasicSc2Bot::BuildFusionCore() {
	const ObservationInterface* observation = Observation();

	// Get Starports
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units starports_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTFLYING));

	// Can't build fusion core without Starports
	if (starports.empty() && starports_f.empty()) {
		return;
	}

	// Build only 1 Fusion core
	Units fusioncore = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));
	if (fusioncore.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 150)) {
		TryBuildStructure(ABILITY_ID::BUILD_FUSIONCORE, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Swaps a Factory with a Starport
void BasicSc2Bot::Swap() {
	if (swappable == true) {
		const ObservationInterface* observation = Observation();
		
		// Get starports and factories and swaps them
		Units factories_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYFLYING));
		Units starports_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTFLYING));

		if (!factories_f.empty() && !starports_f.empty()) {
			const Unit* factory_f = factories_f.front();
			const Unit* starport_f = starports_f.front();

			if (!swap_in_progress) {
				swap_factory_position = factory_f->pos;
				swap_starport_position = starport_f->pos;
				swap_in_progress = true;  
			}

			Actions()->UnitCommand(factory_f, ABILITY_ID::LAND, swap_starport_position);  
			Actions()->UnitCommand(starport_f, ABILITY_ID::LAND, swap_factory_position);  
			swappable = false;
		}
	}
}

