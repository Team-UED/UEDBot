#include "BasicSc2Bot.h"

using namespace sc2;


void BasicSc2Bot::ExecuteBuildOrder() {
	BuildBarracks();
	BuildFactory();
	BuildTechLabAddon();
	BuildStarport();
	Swap();
	BuildFusionCore();
	BuildArmory();
	BuildSecondBase();
}

// Build Barracks if we have a supply depot and enough resources
void BasicSc2Bot::BuildBarracks() {
    const ObservationInterface* observation = Observation();

	Units supply = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));
    Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	
	if (barracks.empty() && !supply.empty() && observation->GetMinerals() >= 150) {
		TryBuildStructure(ABILITY_ID::BUILD_BARRACKS, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build factory if we have a barracks and enough resources
void BasicSc2Bot::BuildFactory() {
	const ObservationInterface* observation = Observation();

	Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units factories_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYFLYING));
	if (!barracks.empty() && factories_f.empty() && factories.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_FACTORY, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build Starport if we have a factory and enough resources
void BasicSc2Bot::BuildStarport() {
	const ObservationInterface* observation = Observation();

	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units starports_f = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTFLYING));
	if (!factories.empty() && starports_f.empty() && starports.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_STARPORT, UNIT_TYPEID::TERRAN_SCV);
	}
}


// Build Tech lab if we have a factory and enough resources
void BasicSc2Bot::BuildTechLabAddon() {
	const ObservationInterface* observation = Observation();

	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	if (!factories.empty() && (observation->GetMinerals() >= 50 && observation->GetVespene() >= 25)) {
		const Unit* factory = factories.front();
		if (factory->orders.empty() && factory->add_on_tag == 0) {
			Actions()->UnitCommand(factory, ABILITY_ID::BUILD_TECHLAB_FACTORY);
		}
	}

}


// Build Fusion Core if we have a starport and enough resources
void BasicSc2Bot::BuildFusionCore() {
	const ObservationInterface* observation = Observation();

	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units fusioncore = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));
	if (!starports.empty() && fusioncore.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 150)) {
		TryBuildStructure(ABILITY_ID::BUILD_FUSIONCORE, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Swaps a factory with a starport
void BasicSc2Bot::Swap() {
	if (swappable == true) {
		const ObservationInterface* observation = Observation();
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

void BasicSc2Bot::BuildSecondBase() {
    const ObservationInterface* observation = Observation();

    // Check if we already have at least two bases.
    Units command_centers = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    if (command_centers.size() >= 2) {
        return;  // We already have two or more bases.
    }

    // Check if a Command Center is already being built.
    Units command_centers_under_construction = observation->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
        return unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER && unit.build_progress < 1.0f;
    });
    if (!command_centers_under_construction.empty()) {
        return;  // A Command Center is already under construction.
    }

    // Get the next expansion location.
    Point3D expansion_location = GetNextExpansion();
    if (expansion_location == Point3D(0.0f, 0.0f, 0.0f)) {
        return;  // No expansion location found.
    }

    // Attempt to build the Command Center at the expansion location.
    TryBuildStructureAtLocation(ABILITY_ID::BUILD_COMMANDCENTER, UNIT_TYPEID::TERRAN_SCV, Point2D(expansion_location));

}
