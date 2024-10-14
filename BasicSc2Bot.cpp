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

void BasicSc2Bot::OnStep() { return; }

void BasicSc2Bot::OnUnitIdle(const Unit* unit) { return; }

void BasicSc2Bot::OnUnitCreated(const Unit* unit) { return; }

void BasicSc2Bot::OnBuildingConstructionComplete(const Unit* unit) { return; }

void BasicSc2Bot::OnUpgradeCompleted(UpgradeID upgrade_id) { return; }

void BasicSc2Bot::OnUnitDestroyed(const Unit* unit) { return; }

void BasicSc2Bot::OnUnitEnterVision(const Unit* unit) { return; }

void BasicSc2Bot::Scout() {}