#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageProduction() {
    TrainMarines();
    TrainBattlecruisers();
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