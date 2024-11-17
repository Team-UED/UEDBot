#include "BasicSc2Bot.h"

BasicSc2Bot::BasicSc2Bot()
    : current_build_order_index(0),
      num_scvs(12),
      num_marines(0),
      num_battlecruisers(0),
	  num_siege_tanks(0),
      is_under_attack(false),
      is_attacking(false),
      need_expansion(false),
      game_time(0.0),
      last_supply_check(0),
      last_expansion_check(0),
      last_scout_time(0),
      enemy_strategy(EnemyStrategy::Unknown),
      swappable(false),
      swap_in_progress(false),
      first_battlecruiser(false),
      is_scouting(false),
	  scout_complete(false),
      current_scout_location_index(0),
      scv_scout(nullptr),
      phase(1),
      nearest_corner_ally(0.0f, 0.0f),
      nearest_corner_enemy(0.0f, 0.0f){
    
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
        ABILITY_ID::BUILD_COMMANDCENTER 
    };
}

void BasicSc2Bot::OnGameStart() {
    // Initialize start locations, expansion locations, chokepoints, etc.
    start_location = Observation()->GetStartLocation();
    retreat_location = { start_location.x + 5.0f, start_location.y };
    enemy_start_locations = Observation()->GetGameInfo().enemy_start_locations;
    if (!enemy_start_locations.empty()) {
        enemy_start_location = enemy_start_locations[0];
    }
    expansion_locations = search::CalculateExpansionLocations(Observation(), Query());

    // Initialize base
	Units command_centers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
    if (!command_centers.empty()) {
        bases.push_back(command_centers.front());
    }

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

    // Get map dimensions for corner coordinates
    const GameInfo& game_info = Observation()->GetGameInfo();
    playable_min = game_info.playable_min;
    playable_max = game_info.playable_max;

    // Initialize the four corners of the map
    map_corners = {
        Point2D(playable_min.x, playable_min.y),  // Bottom-left
        Point2D(playable_max.x, playable_min.y),  // Bottom-right
        Point2D(playable_min.x, playable_max.y),  // Top-left
        Point2D(playable_max.x, playable_max.y)   // Top-right
    };

	// Find the nearest corner to the starting location
    float min_corner_distance = std::numeric_limits<float>::max();;

    for (const auto& corner : map_corners) {
        float corner_distance = DistanceSquared2D(start_location, corner);
        if (corner_distance < min_corner_distance) {
            min_corner_distance = corner_distance;
            nearest_corner_ally = corner;
        }
    }
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

}

void BasicSc2Bot::OnStep() { 
    BasicSc2Bot::ManageEconomy();
    BasicSc2Bot::ExecuteBuildOrder();
    BasicSc2Bot::ManageProduction();
    BasicSc2Bot::ControlUnits();
    BasicSc2Bot::Defense();
    BasicSc2Bot::Offense();
 }

void BasicSc2Bot::OnUnitCreated(const Unit* unit) {
    if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
        num_scvs++;
    }
    if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER) {
		num_battlecruisers++;
    }
    if (unit->unit_type == UNIT_TYPEID::TERRAN_MARINE) {
		num_marines++;
    }
    if (unit->unit_type == UNIT_TYPEID::TERRAN_SIEGETANK) {
		num_siege_tanks++;
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
        ++phase;
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

void BasicSc2Bot::OnUpgradeCompleted(UpgradeID upgrade_id) {
    completed_upgrades.insert(upgrade_id);
}

void BasicSc2Bot::OnUnitDestroyed(const Unit* unit) {

    // Scouting scv died
    if (is_scouting && scv_scout && unit == scv_scout) {

        // find the nearest possible enemy location to its last known position
        float min_distance = std::numeric_limits<float>::max();
        sc2::Point2D nearest_location;

        for (const auto& location : enemy_start_locations) {
            float distance = sc2::Distance2D(scout_location, location);
            if (distance < min_distance) {
                min_distance = distance;
                nearest_location = location;
            }
        }

        // Set the nearest location as the confirmed enemy base location
        enemy_start_location = nearest_location;

        float min_corner_distance = std::numeric_limits<float>::max();
        
		// Find the nearest corner to the enemy base
        for (const auto& corner : map_corners) {
            float corner_distance = DistanceSquared2D(enemy_start_location, corner);
            if (corner_distance < min_corner_distance) {
                min_corner_distance = corner_distance;
                nearest_corner_enemy = corner;
            }
        }

		// Find the corners adjacent to the enemy base
        for (const auto& corner : map_corners) {
            if (corner.x == nearest_corner_enemy.x || corner.y == nearest_corner_enemy.y) {
                enemy_adjacent_corners.push_back(corner);
            }
        }

        // Reset scouting
        scv_scout = nullptr;
		scout_complete = true;
		is_scouting = false;
    }

    if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
		num_scvs--;
    }
    if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER) {
		num_battlecruisers--;
        battlecruiser_retreating[unit] = false;
    }
    if (unit->unit_type == UNIT_TYPEID::TERRAN_MARINE) {
		num_marines--;
    }
    if (unit->unit_type == UNIT_TYPEID::TERRAN_SIEGETANK) {
        num_siege_tanks--;
    }
}

void BasicSc2Bot::OnUnitEnterVision(const Unit* unit) { return; }

// Testing commands
// ./BasicSc2Bot.exe -c -a zerg -d Hard -m CactusValleyLE.SC2Map
// ./BasicSc2Bot.exe -c -a terran -d Hard -m CactusValleyLE.SC2Map
// ./BasicSc2Bot.exe -c -a protoss -d Hard -m CactusValleyLE.SC2Map
// ./BasicSc2Bot.exe -c -a zerg -d Hard -m BelShirVestigeLE.SC2Map
// ./BasicSc2Bot.exe -c -a terran -d Hard -m BelShirVestigeLE.SC2Map
// ./BasicSc2Bot.exe -c -a protoss -d Hard -m BelShirVestigeLE.SC2Map
// ./BasicSc2Bot.exe -c -a zerg -d Hard -m ProximaStationLE.SC2Map
// ./BasicSc2Bot.exe -c -a terran -d Hard -m ProximaStationLE.SC2Map    
// ./BasicSc2Bot.exe -c -a protoss -d Hard -m ProximaStationLE.SC2Map