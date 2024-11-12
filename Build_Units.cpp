#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageProduction() {
    TrainMarines();
    TrainBattlecruisers();
    UpgradeMarines();
    UpgradeBattlecruisers();
}

void BasicSc2Bot::TrainMarines() {
    const ObservationInterface* observation = Observation();
    // Check if we have the resources
    if (observation->GetMinerals() >= 50) {
        // Find Barracks
        Units barracks = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));

        if (!barracks.empty()) {
            const Unit* barrack = barracks.front();
            if (barrack->orders.empty()) {
            // Build a Battlecruiser (One at a time)
            Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
            
            }
        }
    }
}

void BasicSc2Bot::TrainBattlecruisers() {
    const ObservationInterface* observation = Observation();
    // Check if we have the resources
    if (observation->GetMinerals() >= 400 && observation->GetVespene() >= 300) {
        // Find Starports with Tech Labs that ready to build a Battlecruiser
        Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

        if (!starports.empty()) {
            const Unit* starport = starports.front();
            if (starport->add_on_tag != 0) {
                if (starport->orders.empty()) {
                    // Build a Battlecruiser (One at a time)
                    Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BATTLECRUISER);
                }
            }
        }
    }
}

#include <iostream>

void BasicSc2Bot::UpgradeBattlecruisers() {
    const ObservationInterface* observation = Observation();

    // Check if we have a Fusion Core
    Units fusion_cores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));
    if (fusion_cores.empty()) {
        return;
    }

    // Check if Yamato Cannon is already researched
    if (!yamato_cannon_researched && observation->GetMinerals() >= 150 && observation->GetVespene() >= 150) {
        for (const auto& starport : observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT))) {
            if (starport->add_on_tag != 0 && starport->orders.empty()) {
                Actions()->UnitCommand(starport, ABILITY_ID::RESEARCH_BATTLECRUISERWEAPONREFIT);
                yamato_cannon_researched = true;
                break;
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