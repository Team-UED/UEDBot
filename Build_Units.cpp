#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageProduction() {
    TrainMarines();
    TrainBattlecruisers();
    TrainSiegeTanks();
	UpgradeMarines();
	UpgradeMechs();
}

void BasicSc2Bot::TrainMarines() {

    const ObservationInterface* observation = Observation();
    Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
    Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
    Units fusioncores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));

	if (barracks.empty()) {
		return;
	}

    int current_minerals = observation->GetMinerals();

    // Resource costs
    const int marine_mineral_cost = 50;
    const int factory_mineral_cost = 150;
    const int battlecruiser_mineral_cost = 400;

    // Determine if we should train Marines
    bool train = false;

    // Train with base cost in phase 3
    if (phase == 3) {
        if (current_minerals >= marine_mineral_cost) {
            train = true;
        }
    }
    else {
        // Save minerals for Factory if none exists
        if (factories.empty()) {
            int required_minerals = marine_mineral_cost + factory_mineral_cost;
            train = (current_minerals >= required_minerals);
        }
        // Save minerals for Battlecruiser if fusion core exists
        else if (!fusioncores.empty()) {
            int required_minerals = marine_mineral_cost + battlecruiser_mineral_cost;
            train = (current_minerals >= required_minerals);
        }
        // Otherwise, train Marines if there are enough minerals
        else {
            train = (current_minerals >= marine_mineral_cost);
        }
    }

    if (train && !barracks.empty()) {
        const Unit* barrack = barracks.front();
        if (barrack->orders.empty()) {
            // Train a Marine (one at a time)
            Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
        }
    }
}

void BasicSc2Bot::TrainBattlecruisers() {

    // Find Starports to build a Battlecruiser
    const ObservationInterface* observation = Observation();
    Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

    if (starports.empty()) {
        return;
    }

    // Check if we have the resources
    if (observation->GetMinerals() >= 400 &&
        observation->GetVespene() >= 300 &&
        (observation->GetFoodCap() - observation->GetFoodUsed() >= 6)) {

        const Unit* starport = starports.front();

		// Build a Battlecruiser if queue is empty, otherwise check if the train is in progress
        if (starport->add_on_tag != 0) {
            if (starport->orders.empty()) {
                Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BATTLECRUISER);
            }
            else {
                if (starport->orders.front().ability_id == ABILITY_ID::TRAIN_BATTLECRUISER) {
                    if (first_battlecruiser == false) {
                        first_battlecruiser = true;
                        ++phase;
                    }
                }
            }
        }
    }
}


void BasicSc2Bot::TrainSiegeTanks() {
    const ObservationInterface* observation = Observation();
	Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

    Units fusioncores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));

	if (factories.empty()) {
		return;
	}

	if (!fusioncores.empty()) {
        if (observation->GetMinerals() >= 150 && 
            observation->GetVespene() >= 125 &&
            (observation->GetFoodCap() - observation->GetFoodUsed() >= 3)
            ) {

            const Unit* factory = factories.front();
            // Maintain 1 : 4 Ratio of Marines and Siege Tanks
            if (num_marines >= 4 * num_siege_tanks) {
                if (factory->add_on_tag != 0) {
                    if (factory->orders.empty()) {
                        Actions()->UnitCommand(factory, ABILITY_ID::TRAIN_SIEGETANK);
                    }
                }
            }
        }
	}
}

void BasicSc2Bot::UpgradeMarines() {
    const ObservationInterface* observation = Observation();

	// Check if we have research structures (Barracks, Engineering bay)
    Units techlabs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
	Units engineeringbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));

    if (engineeringbays.empty()) {
        return;
    }
  
	// Upgrade from Engineering Bay
    for (const auto& upgrade : engineeringbay_upgrade_order) {
        // Skip if the upgrade is already completed
        if (completed_upgrades.find(upgrade) != completed_upgrades.end()) {
            continue;
        }

        // Get the ability corresponding to the upgrade
        ABILITY_ID ability_id = GetAbilityForUpgrade(upgrade);
        if (ability_id == ABILITY_ID::INVALID) {
            continue;
        }

        // Check if the Engineering Bay is busy or not
        for (const auto& engineeringbay : engineeringbays) {
            if (engineeringbay->orders.empty()) {
                Actions()->UnitCommand(engineeringbay, ability_id);
                return; 
            }
        }
    }

    if (!techlabs.empty()) {
        if (completed_upgrades.find(UPGRADE_ID::COMBATSHIELD) == completed_upgrades.end()) {
            ABILITY_ID upgrade = ABILITY_ID::RESEARCH_COMBATSHIELD;
            // Check if the Tech Lab is busy or not
            for (const auto& techlab : techlabs) {
                if (techlab->orders.empty()) {
                    Actions()->UnitCommand(techlab, upgrade);
                    return;
                }
            }
        }
    }
}

void BasicSc2Bot::UpgradeMechs() {
    const ObservationInterface* observation = Observation();

    // Check if we have research structures (Armory)
    Units armories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ARMORY));

    if (armories.empty() || !first_battlecruiser) {
        return;
    }

    // Upgrade from Armory
    for (const auto& upgrade : armory_upgrade_order) {
        // Skip if the upgrade is already completed
        if (completed_upgrades.find(upgrade) != completed_upgrades.end()) {
            continue;
        }

        // Get the ability corresponding to the upgrade
        ABILITY_ID ability_id = GetAbilityForUpgrade(upgrade);
        if (ability_id == ABILITY_ID::INVALID) {
            continue;
        }

        // Check if the Armory is busy or not
        for (const auto& armory : armories) {
            if (armory->orders.empty()) {
                Actions()->UnitCommand(armory, ability_id);
                return;
            }
        }
    }
}