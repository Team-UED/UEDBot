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
	const ObservationInterface* obs = Observation();
	Units barracks = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
	Units factories = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
	Units starports = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
	Units reactor = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKSREACTOR));

	if (barracks.empty() || phase == 0) {
		return;
	}

	// factory is built...gotta swap
	if (phase == 1 && !factories.empty())
	{
		if (factories.front()->build_progress > 0.5)
		{
			return;
		}
	}
	else if (phase == 2 && !starports.empty())
	{
		if (starports.front()->build_progress > 0.5)
		{
			return;
		}
	}
	if (CanBuild(50) && !barracks.empty()) {

		for (const auto& b : barracks) {
			if (b->orders.empty() && !reactor.empty() && b->add_on_tag == reactor.front()->tag) {
				Actions()->UnitCommand(b, ABILITY_ID::TRAIN_MARINE, true);
				Actions()->UnitCommand(b, ABILITY_ID::TRAIN_MARINE, true);
			}
			else if (b->orders.empty()) {
				Actions()->UnitCommand(b, ABILITY_ID::TRAIN_MARINE, true);
			}
		}
	}
}


void BasicSc2Bot::TrainBattlecruisers() {

	// Find Starports to build a Battlecruiser
	const ObservationInterface* obs = Observation();
	Units starports = obs->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::TERRAN_STARPORT && unit.tag;
		});
	Units fusioncore = obs->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::TERRAN_FUSIONCORE && unit.tag;
		});

	if (starports.empty() || fusioncore.empty()) {
		return;
	}
	const Unit* starport = starports.front();
	if (starport->add_on_tag != 0) {

		if (starport->orders.empty()) {
			if (CanBuild(400, 300, 6)) {
				Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BATTLECRUISER);
			}
		}
		else {
			first_battlecruiser = true;
		}
	}
	// Check if we have the resources

}


void BasicSc2Bot::TrainSiegeTanks() {
	const ObservationInterface* obs = Observation();
	Units factories = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

	Units fusioncores = obs->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FUSIONCORE));

	if (factories.empty()) {
		return;
	}
	const Unit* factory;
	if (CanBuild(150, 125, 3))
	{
		if (phase == 2) {

			factory = factories.front();
			// Maintain 1 : 4 Ratio of Marines and Siege Tanks
			if (num_marines >= 4 * num_siege_tanks) {
				if (factory->add_on_tag != 0) {
					if (factory->orders.empty()) {
						Actions()->UnitCommand(factory, ABILITY_ID::TRAIN_SIEGETANK);
					}
				}
			}
		}
		else if (phase == 3)
		{
			if (first_battlecruiser)
			{
				factory = factories.front();
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