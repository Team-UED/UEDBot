#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageProduction() {
    TrainMarines();
    TrainBattlecruisers();
    UpgradeMarines();
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

    // resource costs
    const int marine_mineral_cost = 50;
    const int factory_mineral_cost = 150;
    const int battlecruiser_mineral_cost = 400;

    // Determine if we should train marines
    bool train = false;

    // Train with base cost in phase 3
    if (phase == 3) {
        if (current_minerals >= marine_mineral_cost) {
            train = true;
        }
    }
    else {
        // Save minerals for factory if none exists
        if (factories.empty()) {
            int required_minerals = marine_mineral_cost + factory_mineral_cost;
            train = (current_minerals >= required_minerals);
        }
        // Save minerals for battlecruiser if fusion core exists
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

    const ObservationInterface* observation = Observation();
    // Find Starports with Tech Labs that ready to build a Battlecruiser
    Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

    if (starports.empty()) {
        return;
    }

    // Check if we have the resources
    if (observation->GetMinerals() >= 400 && observation->GetVespene() >= 300) {
        // Find Starports with Tech Labs that ready to build a Battlecruiser
        if (!starports.empty()) {
            const Unit* starport = starports.front();
            if (starport->add_on_tag != 0) {
                if (starport->orders.empty()) {
                    // Build a Battlecruiser (One at a time)
                    Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BATTLECRUISER);
					if (!first_battlecruiser) {
						phase++;
						first_battlecruiser = true;
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
