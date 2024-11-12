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
	BuildArmory();
}

// Build Barracks if we have a supply depot and enough resources
void BasicSc2Bot::BuildBarracks() {
    const ObservationInterface* observation = Observation();

	// Get supply depots
	Units supply = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));

	// Can't built barracks without supply depots
	if (supply.empty()) {
		return; 
	}

	// Build only 1 barrack
    Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	if (barracks.empty() && observation->GetMinerals() >= 150) {
		TryBuildStructure(ABILITY_ID::BUILD_BARRACKS, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Engineering bay if we have a barrack and enough resources
void BasicSc2Bot::BuildEngineeringBay() {
	const ObservationInterface* observation = Observation();

	// Get barracks
	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	// Get factories
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	// Can't build engineering bay without barracks
	if (barracks.empty()) {
		return;
	}

	// Build only 1 engineering bay (After factory)
	Units engineeringbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
	if (engineeringbays.empty() && observation->GetMinerals() >= 125 && !factories.empty()) {
		TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Orbital Command if we have a command center and enough resources
void BasicSc2Bot::BuildOrbitalCommand() {
	const ObservationInterface* observation = Observation();

	// Get engineering bays
	Units engineeringbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
	// Can't build orbital command without engineering bay
	if (engineeringbays.empty()) {
		return; 
	}

	// Find a Command Center that can be upgraded
	Units command_centers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
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




// Build factory if we have a barracks and enough resources
void BasicSc2Bot::BuildFactory() {
	const ObservationInterface* observation = Observation();

	// Get barracks
	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));

	// Can't build factory without barracks
	if (barracks.empty()) {
		return;
	}

	// Build only 1 factory
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units factories_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYFLYING));
	if (factories_f.empty() && factories.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_FACTORY, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build Starport if we have a factory and enough resources
void BasicSc2Bot::BuildStarport() {
	const ObservationInterface* observation = Observation();

	// Get factories
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	// Can't build starports without factories
	if (factories.empty()) {
		return;
	}

	// Build only 1 starport
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units starports_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTFLYING));
	if (starports_f.empty() && starports.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_STARPORT, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build Tech lab if we have a factory and enough resources
void BasicSc2Bot::BuildTechLabAddon() {
	const ObservationInterface* observation = Observation();

	// Get factories
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	// Can't build techlab without factories
	if (factories.empty()) {
		return;
	}

	// Build addon
	if ((observation->GetMinerals() >= 50 && observation->GetVespene() >= 25)) {
		const Unit* factory = factories.front();
		if (factory->orders.empty() && factory->add_on_tag == 0) {
			Actions()->UnitCommand(factory, ABILITY_ID::BUILD_TECHLAB_FACTORY);
		}
	}

}


// Build Fusion Core if we have a starport and enough resources
void BasicSc2Bot::BuildFusionCore() {
	const ObservationInterface* observation = Observation();

	// Get starports
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units starports_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTFLYING));

	// Can't build fusion core without starports
	if (starports.empty() && starports_f.empty()) {
		return;
	}

	// Build only 1 fusion core
	Units fusioncore = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));
	if (fusioncore.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 150)) {
		TryBuildStructure(ABILITY_ID::BUILD_FUSIONCORE, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Swaps a factory with a starport
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

// Bot must build a Armory to upgrade units
// Builds an Armory to enable unit upgrades, ensuring prerequisites are met
void BasicSc2Bot::BuildArmory() {
    const ObservationInterface* observation = Observation();

    // Check if Armory already exists or is being built
    Units armories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ARMORY));
    bool armory_being_built = false;
    for (const auto& task : build_tasks) {
        if (task.unit_type == UNIT_TYPEID::TERRAN_ARMORY && !task.is_complete) {
            armory_being_built = true;
            break;
        }
    }

    if (!armories.empty() || armory_being_built) {
        return; // Armory already exists or is in progress
    }

    // Ensure prerequisites: at least one Factory with a Tech Lab
    Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
    bool has_factory_with_techlab = false;

    for (const auto& factory : factories) {
        if (factory->add_on_tag != 0) {
			const Unit* add_on = observation->GetUnit(factory->add_on_tag);
			if (add_on->unit_type == UNIT_TYPEID::TERRAN_TECHLAB) {
				has_factory_with_techlab = true;
				break;
			}
		}
    }

    if (!has_factory_with_techlab) {
        return; // Prerequisite not met
    }

    // Check resource availability
    if (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100) {
        TryBuildStructure(ABILITY_ID::BUILD_ARMORY, UNIT_TYPEID::TERRAN_SCV);
    }
}