#include "BasicSc2Bot.h"

using namespace sc2;


void BasicSc2Bot::ExecuteBuildOrder() {
	BuildBarracks();
	BuildFactory();
    BuildStarport();
	BuildTechLabAddon();
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
	if (!barracks.empty() && factories.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_FACTORY, UNIT_TYPEID::TERRAN_SCV);
	}
}

// Build Starport if we have a factory and enough resources
void BasicSc2Bot::BuildStarport() {
	const ObservationInterface* observation = Observation();

	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	if (!factories.empty() && starports.empty() && (observation->GetMinerals() >= 150 && observation->GetVespene() >= 100)) {
		TryBuildStructure(ABILITY_ID::BUILD_STARPORT, UNIT_TYPEID::TERRAN_SCV);
	}
}

void BasicSc2Bot::BuildTechLabAddon() {
	const ObservationInterface* observation = Observation();

	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

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