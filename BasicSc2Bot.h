#ifndef BASIC_SC2_BOT_H_
#define BASIC_SC2_BOT_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "sc2api/sc2_unit.h"
#include "cpp-sc2/include/sc2api/sc2_interfaces.h"
#include "sc2api/sc2_unit_filters.h"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <map>

using namespace sc2;

// Add the hash specialization for sc2::AbilityID
template <>
struct std::hash<sc2::AbilityID> {
    size_t operator()(const sc2::AbilityID& ability_id) const noexcept {
        return std::hash<uint32_t>()(static_cast<uint32_t>(ability_id));
    }
};

class BasicSc2Bot : public sc2::Agent {
public:
    // Constructors
    BasicSc2Bot();

    // SC2 API Event Methods
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnGameEnd() final;
    virtual void OnUnitCreated(const Unit* unit) final;
    virtual void OnBuildingConstructionComplete(const Unit* unit) final;
    virtual void OnUpgradeCompleted(UpgradeID upgrade_id) final;
    virtual void OnUnitDestroyed(const Unit* unit) final;
    virtual void OnUnitEnterVision(const Unit* unit) final;

private:
    // =========================
    // Economy Management
    // =========================

    // Manages all economic aspects, called each step.
    void ManageEconomy();

    // Trains SCVs continuously until all bases have maximum workers.
    void TrainSCVs();

    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);

    // Builds additional supply depots to avoid supply blocks.
    bool TryBuildSupplyDepot();

    // Builds refineries early and assigns SCVs to gather gas.
    void BuildRefineries();

    void TryBuildRefinery(const Unit* geyser);

    // Assigns idle workers to mineral patches or gas.
    void AssignWorkers();

    // Expands to a new base when needed.
    void BuildExpansion();

    // Reassigns workers to the closest mineral patch or gas.
    void ReassignWorkers();

	// Call down MULEs to gather resources
    void UseMULE();

	// Phase of the strategy
	// Phase 1 -> Start of the game ~ until the first star port is built
	// Phase 2 -> First star port is built ~ until the first battlecruiser is built
	// Phase 3 -> First battlecruiser is built ~ rest of the game
    int phase;

    // =========================
    // Build Order Execution
    // =========================

    // Executes the build order step by step.
    void ExecuteBuildOrder();

    // Builds a Barracks as the first production structure.
    void BuildBarracks();

    // Builds a Factory as the second production structure.
    void BuildFactory();

    // Builds a Starport as the third production structure.
    void BuildStarport();

    // Builds a Tech Lab addon and swaps it with the Starport.
    void BuildTechLabAddon();

    // Builds a Fusion Core to enable Battlecruiser production.
    void BuildFusionCore();

    // Builds a second base (Command Center).
    void BuildSecondBase();

    // Builds an Engineering bay
    void BuildEngineeringBay();

	// Builds an Orbital Command
    void BuildOrbitalCommand();

    // Flag to track Tech Lab building progress
    bool techlab_building_in_progress = false;

    // Pointer to track the current Factory in use
    const Unit* current_factory = nullptr;     

    // Swaps building
    void Swap();

    // Check if swap is possible
    bool swappable;

    // Check if swap is in progress
    bool swap_in_progress;

    // Swpas location of 2 structures
    Point2D swap_factory_position;
    Point2D swap_starport_position;


    // =========================
    // Unit Production and Upgrades
    // =========================

    // Manages production of units and upgrades.
    void ManageProduction();

    // Trains Marines for early defense.
    void TrainMarines();

    // Trains Battlecruisers as fast as possible.
    void TrainBattlecruisers();

    // Trains Siege Tanks for later defense.
    void TrainSiegeTanks();

    // Upgrades Marines.
    void UpgradeMarines();

    // Manages research of upgrades.
    void ManageUpgrades();

	// Tracks if train of the first battlecruiser is in progress
    bool first_battlecruiser;

	// Retreating flag
    std::unordered_map<const Unit*, bool> battlecruiser_retreating;

    // Retreating flag for diagonal opposite
    std::unordered_map<const Unit*, bool> battlecruiser_reached_adj_corner;

	// Retreating location
    std::unordered_map<const Unit*, Point2D> battlecruiser_retreat_location;

    // Retreating location for diagonal opposite (go to adjacent vertex first)
    std::unordered_map<const Unit*, Point2D> battlecruiser_adj_corner;


    // =========================
    // Defense Management
    // =========================

    // Manages defensive structures and units.
    void Defense();

    // Blocks the chokepoint with buildings for early defense.
    void BlockChokepoint();

    // Defends against early rushes using Marines and SCVs if necessary.
    void EarlyDefense();

    // Builds additional defense structures like missile turrets.
    void LateDefense();

    // =========================
    // Offense Management
    // =========================

    // Manages offensive actions.
    void Offense();

    // Executes an all-out rush with all available units.
    void AllOutRush();

    // =========================
    // Unit Control
    // =========================

    // Controls all units (SCVs, Marines, Battlecruisers).
    void ControlUnits();

    // Controls SCVs during dangerous situations and repairs.
    void ControlSCVs();

    // Manages SCVs during dangerous situations.
    void RetreatFromDanger();

    // Repairs damaged units during or after engagements.
    void RepairUnits();

    // Repairs damaged structures during enemy attacks.
    void RepairStructures();

    // SCVs attack in urgent situations (e.g., enemy attacking the main base).
    void SCVAttackEmergency();

    // SCVs scouting enemy base.
    void SCVScout();

    // Controls Marines with micro (kiting, focus fire).
    void ControlMarines();

    // Controls Battlecruisers (abilities, targeting, positioning).
    void ControlBattlecruisers();

    // Controls Battlecruisers to jump into enemy base
    void Jump();

	// Controls Battlecruisers to target enemy units
    void Target();

    // Controls Battlecruisers to retreat
    void Retreat(const Unit* unit);

	// Check if retreating is complete
    void RetreatCheck();

    // Controls Siege Tanks (abilities, targeting, positioning).
    void ControlSiegeTanks();

	// Controls Siege Tanks (temp)
	void SiegeMode();

    // Scv that is scouting
    const sc2::Unit* scv_scout;

	// Flag to check if SCV is scouting
    bool is_scouting;

    // Flag to check if scout is finished
    bool scout_complete;

	// Track visited enemy base locations
    int current_scout_location_index;

    // Track location of scouting scv
    sc2::Point2D scout_location;

	// Location of enemy base based on ally base location
	// (0 = same vertical, 1 = same horizontal, 2 = opposite diagonal)
    int enemy_location;

    // Playable width and height of the map
    Point2D playable_min;
    Point2D playable_max;

    // Four corners of the map
    std::vector<Point2D> map_corners;


    // =========================
    // Helper Methods
    // =========================

    // Scouting methods to gather information about the enemy.

    // Updates the game state (e.g., under attack, need expansion).
    void UpdateGameState();

    // Manages supply to avoid supply blocks.
    void ManageSupply();

    // Manages army units and their composition.
    void ManageArmy();

    // Checks if the bot is supply blocked.
    bool IsSupplyBlocked() const;

    // Checks if an expansion is needed.
    bool NeedExpansion() const;

    // Gets the next available expansion location.
    Point3D GetNextExpansion() const;

    // Checks if the enemy is rushing.
    bool IsEnemyRushing() const;

    // Find a unit of a given type.
    const Unit* FindUnit(UnitTypeID unit_type) const;

    // Get the main base.
    const Unit* GetMainBase() const;

    // Get the newest base.
    const Unit* GetNewestBase() const;

    // Returns the count of units of a given type.
    size_t CountUnitType(UnitTypeID unit_type) const;

    // Returns the count of units being built of a given type.
    size_t CountUnitTypeBuilding(UnitTypeID unit_type) const;

    // Returns true if the ability is being researched or built.
    bool IsAbilityInProgress(AbilityID ability_id) const;

    // Returns true if the position is dangerous. (e.g., enemy units nearby)
    bool IsDangerousPosition(const Point2D &pos);

    // Gets the closest safe position for SCVs. (e.g., towards the main base)
    Point2D GetSafePosition();

    // Finds the closest damaged unit for repair.
    const Unit *FindDamagedUnit();

    // Finds the closest damaged structure for repair.
    const Unit *FindDamagedStructure();

    // Returns true if the main base is under attack.
    bool IsMainBaseUnderAttack();

    // Finds the closest enemy unit to a given position.
    const Unit *FindClosestEnemy(const Point2D &pos);

    bool TryBuildStructureAtLocation(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type, const Point2D& location);

    Point2D GetRallyPoint();

    const Unit *GetLeastSaturatedBase() const;

    bool IsWorkerUnit(const Unit* unit);

    

    // =========================
    // Member Variables
    // =========================

    // Build order queue.
    std::vector<AbilityID> build_order;
    size_t current_build_order_index;

    // Keeps track of unit counts.
    size_t num_scvs;
    size_t num_marines;
    size_t num_battlecruisers;
	size_t num_siege_tanks;

    // Map information.
    sc2::Point2D start_location;
    sc2::Point2D enemy_start_location;
	std::vector<sc2::Point2D> enemy_start_locations;
    std::vector<sc2::Point3D> expansion_locations;

    // Our bases.
    Units bases;

    // Our army units.
    Units army_units;

    // Enemy units we've seen.
    Units enemy_units;

    // Flags for game state.
    bool is_under_attack;
    bool is_attacking;
    bool need_expansion;

    // Timing variables.
    double game_time;

    // For managing supply depots.
    size_t last_supply_check;

    // For managing expansions.
    size_t last_expansion_check;

    // For managing scouting.
    size_t last_scout_time;

    // For controlling specific units.
    Units battlecruisers;

    // Chokepoint locations.
    std::vector<Point2D> chokepoints;

    // Enemy strategy detected.
    enum class EnemyStrategy {
        Unknown,
        EarlyRush,
        AirUnits,
        GroundUnits
    };
    EnemyStrategy enemy_strategy;

    // Unit Filters
    const std::vector<UnitTypeID> mineral_fields;
    const std::vector<UnitTypeID> vespene_geysers;

    // Stores possible places that bases can be built.
    std::vector<sc2::Point3D> possible_expansions;

    // For storing pending actions to prevent duplicates.
    std::unordered_set<AbilityID> pending_actions;

    // For managing specific build tasks.
    struct BuildTask {
        UnitTypeID unit_type;
        AbilityID ability_id;
        bool is_complete;
    };
    std::vector<BuildTask> build_tasks;

    // For microing units.
    std::unordered_map<Tag, const Unit*> unit_micro_map;

    // For managing repairs.
    std::unordered_set<Tag> scvs_repairing;

    // For managing SCV safety.
    std::unordered_set<Tag> scvs_under_attack;

    // For tracking enemy units.
    std::unordered_map<Tag, const Unit*> enemy_unit_map;

    // Map of threat levels for specific anti-air units
    const std::unordered_map<sc2::UNIT_TYPEID, int> threat_levels = {
        {sc2::UNIT_TYPEID::TERRAN_MARINE, 1},
        {sc2::UNIT_TYPEID::TERRAN_GHOST, 1},
        {sc2::UNIT_TYPEID::TERRAN_CYCLONE, 2},
        {sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, 2},
        {sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, 2},
        {sc2::UNIT_TYPEID::TERRAN_THOR, 4},
        {sc2::UNIT_TYPEID::TERRAN_MISSILETURRET, 3},
        {sc2::UNIT_TYPEID::PROTOSS_STALKER, 3},
        {sc2::UNIT_TYPEID::PROTOSS_SENTRY, 1},
        {sc2::UNIT_TYPEID::PROTOSS_ARCHON, 3},
        {sc2::UNIT_TYPEID::PROTOSS_PHOENIX, 2},
        {sc2::UNIT_TYPEID::PROTOSS_VOIDRAY, 4},
        {sc2::UNIT_TYPEID::PROTOSS_CARRIER, 3},
        {sc2::UNIT_TYPEID::PROTOSS_TEMPEST, 2},
        {sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, 3},
        {sc2::UNIT_TYPEID::ZERG_QUEEN, 2},
        {sc2::UNIT_TYPEID::ZERG_HYDRALISK, 2},
        {sc2::UNIT_TYPEID::ZERG_MUTALISK, 2},
        {sc2::UNIT_TYPEID::ZERG_CORRUPTOR, 4},
        {sc2::UNIT_TYPEID::ZERG_SPORECRAWLER, 3}
    };
};

#endif