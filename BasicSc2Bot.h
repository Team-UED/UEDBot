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

using namespace sc2;

class BasicSc2Bot : public sc2::Agent {
public:
    // Constructors
    BasicSc2Bot();

    // SC2 API Event Methods
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnGameEnd() final;
    virtual void OnUnitIdle(const Unit* unit) final;
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

    // Builds additional supply depots to avoid supply blocks.
    void BuildSupplyDepots();

    // Builds refineries early and assigns SCVs to gather gas.
    void BuildRefineries();

    // Assigns idle workers to mineral patches or gas.
    void AssignWorkers();

    // Expands to a new base when needed.
    void BuildExpansion();

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

    // Builds an Armory to upgrade units.
    void BuildArmory();

    // Builds a second base (Command Center).
    void BuildSecondBase();

    // =========================
    // Unit Production and Upgrades
    // =========================

    // Manages production of units and upgrades.
    void ManageProduction();

    // Trains Marines for early defense.
    void TrainMarines();

    // Trains Battlecruisers as fast as possible.
    void TrainBattlecruisers();

    // Upgrades Battlecruisers
    void UpgradeBattlecruisers();

    // Upgrades Marines.
    void UpgradeMarines();

    // Manages research of upgrades.
    void ManageUpgrades();

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

    // Executes the Battlecruiser rush strategy.
    void BattlecruiserRush();

    // Executes an all-out rush with all available units.
    void AllOutRush();

    // =========================
    // Unit Control
    // =========================

    // Controls all units (SCVs, Marines, Battlecruisers).
    void ControlUnits();

    // Controls SCVs during dangerous situations and repairs.
    void ControlSCVs();

    // Controls Marines with micro (kiting, focus fire).
    void ControlMarines();

    // Controls Battlecruisers (abilities, targeting, positioning).
    void ControlBattlecruisers();

    // =========================
    // Helper Methods
    // =========================

    // Scouting methods to gather information about the enemy.
    void Scout();

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

    // Checks if we can afford to build/train something.
    bool CanAfford(UnitTypeID unit_type) const;
    bool CanAfford(AbilityID ability_id) const;

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

    // Assigns idle SCVs to minerals or gas.
    void AssignIdleWorkers();

    // Returns true if the ability is being researched or built.
    bool IsAbilityInProgress(AbilityID ability_id) const;

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

    // Map information.
    sc2::Point2D start_location;
    sc2::Point2D enemy_start_location;
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

    // For tracking upgrades.
    bool yamato_cannon_researched;

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
};

#endif