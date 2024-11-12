#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageProduction() {
    //TrainMarines();
    TrainBattlecruisers();
}
void BasicSc2Bot::TrainBattlecruisers() {
    const ObservationInterface* observation = Observation();
    // Check if we have the resources
    if (observation->GetMinerals() >= 400 && observation->GetVespene() >= 300) {
        // Find Starports with Tech Labs that ready to build a Battlecruiser
        Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
        for (const auto& starport : starports) {
            // Ensure the Starport has a Tech Lab
            if (starport->add_on_tag != 0) {
                const Unit* add_on = observation->GetUnit(starport->add_on_tag);
                if (add_on && add_on->unit_type == UNIT_TYPEID::TERRAN_TECHLAB && starport->orders.empty()) {
                    // Build a Battlecruiser (One at a time)
                    Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BATTLECRUISER);
                    break; 
                }
            }
        }
    }
}