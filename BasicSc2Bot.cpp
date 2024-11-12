#include "BasicSc2Bot.h"

BasicSc2Bot::BasicSc2Bot()
    : current_build_order_index(0),
      num_scvs(0),
      num_marines(0),
      num_battlecruisers(0),
      is_under_attack(false),
      is_attacking(false),
      need_expansion(false),
      game_time(0.0),
      last_supply_check(0),
      last_expansion_check(0),
      last_scout_time(0),
      yamato_cannon_researched(false),
      enemy_strategy(EnemyStrategy::Unknown),
      swappable(false),
      swap_in_progress(false),
      first_battlecruiser(false){
    
    build_order = {
        ABILITY_ID::BUILD_SUPPLYDEPOT,
        ABILITY_ID::BUILD_BARRACKS,
        ABILITY_ID::BUILD_REFINERY,
        ABILITY_ID::BUILD_ENGINEERINGBAY,
        ABILITY_ID::BUILD_BUNKER,
        ABILITY_ID::BUILD_FACTORY,
        ABILITY_ID::BUILD_STARPORT,
        ABILITY_ID::BUILD_TECHLAB,
        ABILITY_ID::BUILD_FUSIONCORE,
        ABILITY_ID::BUILD_ARMORY,
        ABILITY_ID::BUILD_COMMANDCENTER 
    };
}

void BasicSc2Bot::OnGameStart() {
    // Initialize start locations, expansion locations, chokepoints, etc.
    start_location = Observation()->GetStartLocation();
    enemy_start_location = Observation()->GetGameInfo().enemy_start_locations[0];
    expansion_locations = search::CalculateExpansionLocations(Observation(), Query());

    // Initialize base
	Units command_centers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
    bases.push_back(command_centers.front());

    // Initialize chokepoints (Why push mineral to the base?)
    //for (auto& expansion : expansion_locations) {
    //    Units units = Observation()->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_MINERALFIELD));
    //    if (!units.empty()) {
    //        bases.push_back(units.front());
    //    }
    //}

    // Initialize build tasks based on build order
    for (auto ability : build_order) {
        BuildTask task;
        task.ability_id = ability;
        task.is_complete = false;
        build_tasks.push_back(task);
    }

    // Initialize other game state variables
    is_under_attack = false;
    is_attacking = false;
    need_expansion = false;
    yamato_cannon_researched = false;

}

void BasicSc2Bot::OnGameEnd() { 
    // Get the game info
    const ObservationInterface* observation = Observation();
    GameInfo gameInfo = observation->GetGameInfo();
    auto players = std::map<uint32_t, PlayerInfo*>();
    for (auto& playerInfo : gameInfo.player_info) {
        players[playerInfo.player_id] = &playerInfo;
    }

    // Map the player types to strings
    auto playerTypes = std::map<PlayerType, std::string>();
    playerTypes[PlayerType::Participant] = "UED";
    playerTypes[PlayerType::Computer] = "Prey";
    playerTypes[Observer] = "Observer";

    // Map the game results to strings
    auto gameResults = std::map<GameResult, std::string>();
    gameResults[Win] = " Wins";
    gameResults[Loss] = " Loses";
    gameResults[Tie] = " Tied";
    gameResults[Undecided] = " Undecided";

    // Print the results of the game
    for (auto& playerResult : observation->GetResults()) {
    std::cout << playerTypes[((*(players[playerResult.player_id])).player_type)]
                << gameResults[playerResult.result]
                << std::endl;
    }
}

void BasicSc2Bot::OnStep() { 
    BasicSc2Bot::ManageEconomy();
    BasicSc2Bot::ExecuteBuildOrder();
    BasicSc2Bot::ManageProduction();
    BasicSc2Bot::ControlUnits();
 }

void BasicSc2Bot::OnUnitCreated(const Unit* unit) {
    if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER) {
        first_battlecruiser == true;
	}
}

void BasicSc2Bot::OnBuildingConstructionComplete(const Unit* unit) {
    if (unit->unit_type == UNIT_TYPEID::TERRAN_REFINERY) {
        const ObservationInterface* observation = Observation();

		// Get all SCVs that are free
        Units scvs = observation->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
            return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && !unit.orders.empty() &&
                unit.orders.front().ability_id == ABILITY_ID::HARVEST_GATHER;
            });

        // Assign them to gas
        int scv_count = 0;
        for (const auto& scv : scvs) {
            // Assign SCV to the refinery
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, unit); 
            scv_count++;
            // Stop after assigning 3 SCVs
            if (scv_count >= 3) break;
        }
    }

    if (unit->unit_type == UNIT_TYPEID::TERRAN_STARPORT) {
        const ObservationInterface* observation = Observation();
        Units factories = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
		Units starports = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
		if (!factories.empty() && !starports.empty()) {
            const Unit* factory = factories.front();
            const Unit* starport = starports.front();
            Actions()->UnitCommand(factory, ABILITY_ID::LIFT);
            Actions()->UnitCommand(starport, ABILITY_ID::LIFT);
            swappable = true;
		}
    }
}

void BasicSc2Bot::OnUpgradeCompleted(UpgradeID upgrade_id) { }

void BasicSc2Bot::OnUnitDestroyed(const Unit* unit) { return; }

void BasicSc2Bot::OnUnitEnterVision(const Unit* unit) { return; }