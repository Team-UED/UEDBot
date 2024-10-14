#include "BasicSc2Bot.h"

void BasicSc2Bot::OnGameStart() { return; }

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
    TryBuildSupplyDepot();
    TryBuildBarracks();
 }

void BasicSc2Bot::OnUnitIdle(const Unit* unit) {
    // Called when a unit becomes idle.
    const ObservationInterface* observation = Observation();

    auto unit_type = unit->unit_type.ToType();

    if (unit_type == UNIT_TYPEID::TERRAN_SCV) {
        // If an SCV is idle, send it to gather minerals.
        const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
        if (!mineral_target) {
            return;
        }
        Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
    }
    else if (unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER) {
        // If the Command Center is idle, train an SCV if we have less than 15.
        if (CountUnitType(UNIT_TYPEID::TERRAN_SCV) < 15) {
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
        }
    }
    else if (unit_type == UNIT_TYPEID::TERRAN_BARRACKS) {
        // If the Barracks is idle, train a Marine.
        Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
    }
}


bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type) {
    // Attempts to build a structure with an available SCV.
    const ObservationInterface* observation = Observation();

    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));

    for (const auto& unit : units) {
        // Use the first idle SCV found.
        if (unit->orders.empty()) {
            unit_to_build = unit;
            break;
        }
    }

    if (unit_to_build) {
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        Actions()->UnitCommand(unit_to_build,
                               ability_type_for_structure,
                               Point2D(unit_to_build->pos.x + rx * 15.0f,
                                       unit_to_build->pos.y + ry * 15.0f));
        return true;
    }
    return false;
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
    // Try to build a supply depot.
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Self);
    int32_t supply = observation->GetFoodUsed();
    if (supply >= observation->GetFoodCap() - 2) {
        return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
    }
    return false;
}

bool BasicSc2Bot::TryBuildBarracks() {
    // Try to build a barracks.
    const ObservationInterface* observation = Observation();

    if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
        return false;  // Already have a Barracks.
    }

    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
}

size_t BasicSc2Bot::CountUnitType(UNIT_TYPEID unit_type) {
    // Counts the number of units of a specific type.
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
}

void BasicSc2Bot::Scout(const Unit* unit) {
    // Very simple scouting function.
    const GameInfo& game_info = Observation()->GetGameInfo();
    auto enemy_start_locations = game_info.enemy_start_locations;
    Actions()->UnitCommand(unit, ABILITY_ID:: GENERAL_PATROL, GetRandomEntry(enemy_start_locations));
}

const Unit* BasicSc2Bot::FindNearestMineralPatch(const Point2D& start) {
    // Finds the nearest mineral patch to a given point.
    Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const Unit* target = nullptr;
    for (const auto& u : units) {
        if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD ||
            u->unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD) {
            float d = DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
    }
    return target;
}