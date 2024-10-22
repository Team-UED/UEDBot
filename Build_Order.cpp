#include "BasicSc2Bot.h"

using namespace sc2;


void BasicSc2Bot::ExecuteBuildOrder() {
	BuildBarracks();
	BuildFactory();
	BuildTechLabAddon();
	BuildStarport();
	Swap();
	BuildFusionCore();
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
