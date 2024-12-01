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

	if (armories.empty() || fusioncores.empty() || !first_battlecruiser) {
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