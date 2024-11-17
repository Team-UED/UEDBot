#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageProduction() {
    TrainMarines();
    TrainBattlecruisers();
    TrainSiegeTanks();
    UpgradeMarines();
    UpgradeSiegeTanksAndBattleCruisers();
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
            // Find Factories with Tech Labs that ready to build a Siege Tank
            if (!factories.empty()) {
                const Unit* factory = factories.front();
                if (factory->add_on_tag != 0) {
                    if (factory->orders.empty()) {
                        // Build a Siege Tank (One at a time)
                        Actions()->UnitCommand(factory, ABILITY_ID::TRAIN_SIEGETANK);
                    }
                }
            }
        }
	}
}

void BasicSc2Bot::UpgradeMarines() {
    const ObservationInterface* observation = Observation();

    // Check if we have an Engineering Bay
    Units engineering_bays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
    if (engineering_bays.empty()) {
        return;
    }

    // Check if Stim Pack is already researched
    if (observation->GetMinerals() >= 100 && observation->GetVespene() >= 100) {
        for (const auto& barracks : observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS))) {
            if (barracks->orders.empty() && std::find(observation->GetUpgrades().begin(), observation->GetUpgrades().end(), UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1) == observation->GetUpgrades().end()) {
                Actions()->UnitCommand(barracks, ABILITY_ID::RESEARCH_STIMPACK);
                break;
            }
        }
    }
}

void BasicSc2Bot::UpgradeSiegeTanksAndBattleCruisers() {
    const ObservationInterface* observation = Observation();

    // Check if we have an Armory and Fusion Core
    Units armories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ARMORY));
    Units fusioncores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));

    if (armories.empty() && fusioncores.empty() && !first_battlecruiser) {
        return;
    }

    // Order of upgrades for Armory
    std::vector<UPGRADE_ID> armory_upgrade_order = {
        UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1,
        UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1,
        UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL2,
        UPGRADE_ID::TERRANSHIPWEAPONSLEVEL2,
        UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL3,
        UPGRADE_ID::TERRANSHIPWEAPONSLEVEL3,
        UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1,
        UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL2,
        UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL3
    };


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

    if (completed_upgrades.find(UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS) == completed_upgrades.end()) {
        
        ABILITY_ID upgrade = ABILITY_ID::RESEARCH_BATTLECRUISERWEAPONREFIT;
        // Check if the Fusion Core is busy or not
        for (const auto& fusioncore : fusioncores) {
            if (fusioncore->orders.empty()) {
                Actions()->UnitCommand(fusioncore, upgrade);
                return; 
            }
        }
    }
}