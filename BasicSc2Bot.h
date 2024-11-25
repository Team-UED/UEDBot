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
#include "sc2api/sc2_map_info.h"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <string>
#include <cmath>
#include <map>
#include <memory>

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
	// Debugging
	// =========================
	void Debugging();
	void DrawBoxesOnMap(sc2::DebugInterface* debug, int map_width, int map_height);
	void DrawBoxAtLocation(sc2::DebugInterface* debug, const sc2::Point3D& location, float size, const sc2::Color& color = sc2::Colors::Red) const;

	uint32_t current_gameloop;
	uint32_t last_gameloop;

	// test
	void CancelTraining();

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
	// phase 0 -> Start of the game ~ until the first barracks with techlab is built
	// Phase 1 -> ~ until first factory is built and swapped
	// Phase 2 -> ~ until star port and swapped + first battlecruiser is built
	// Phase 3 -> ~ rest of the game
	int phase = 0;

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

	// Builds a Armory to upgrate units.
	void BuildArmory();

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
	void Swap(const Unit* a, const Unit* b, bool lift);

	// Check if swap is in progress
	bool swap_in_progress = false;

	// Swap buildings
	const Unit* swap_a = nullptr;
	const Unit* swap_b = nullptr;


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

	// Upgrades Siege Tanks
	void UpgradeSiegeTanksAndBattleCruisers();

	// Manages research of upgrades.
	void ManageUpgrades();

	// Tracks if train of the first battlecruiser is in progress
	bool first_battlecruiser;

	// =========================
	// Defense Management
	// =========================

	// Manages defensive structures and units.
	void Defense();

	// Blocks the chokepoint with buildings for early defense.
	//void BlockRamp();

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

	// Updates the amoount of SCVs repairing a unit.
	void UpdateRepairingSCVs();

	// SCVs attack in urgent situations (e.g., enemy attacking the main base).
	void SCVAttackEmergency();

	// SCVs scouting enemy base.
	void SCVScoutEnemySpawn();

	// SCV scout entire map
	void SCVScoutMapInit();

	// Updates SCV scouting status
	void UpdateSCVScouting();

	// Controls Battlecruisers (abilities, targeting, positioning).
	void ControlBattlecruisers();

	// Controls Battlecruisers to jump into enemy base
	void Jump();

	// Controls Battlecruisers to target enemy units
	void TargetBattlecruisers();

	// Controls Battlecruisers to retreat
	void Retreat(const Unit* unit);

	// Check if retreating is complete
	void RetreatCheck();

	// Targets unit for Battlecruisers to use Yamato Cannon
	void UseYamatoCannon(const Unit* battlecruiser, const Units& enemy_units, std::set<Tag>& yamato_targets);

	// Controls Siege Tanks (abilities, targeting, positioning).
	void ControlSiegeTanks();

	// Controls Siege Tanks (temp)
	void SiegeMode();

	// Controls SiegeTanks to target enemy units
	void TargetSiegeTank();

	// Controls Marines with micro (kiting, focus fire).
	void ControlMarines();

	// Controls Marines to target enemy units
	void TargetMarines();

	// Use stimpack ability for Marines
	void UseStimPack(const Unit* marine);

	// SCV that is building
	const sc2::Unit* scv_building;

	// SCV that is scouting
	const sc2::Unit* scv_scout;

	// Flag to check if SCV is scouting
	bool is_scouting;

	// Grid points to scout
	std::vector<Point2D> scout_points;

	// Flag to check if scout is finished
	bool scout_complete;

	// Track visited enemy base locations
	int current_scout_location_index;

	// Track visted map locations
	int current_scout_index = 0; // Current index of scout points

	// Track location of scouting SCV
	sc2::Point2D scout_location;

	// Playable width and height of the map
	Point2D playable_min;
	Point2D playable_max;

	// Four corners of the map
	std::vector<Point2D> map_corners;

	// Corner closest to ally base
	Point2D nearest_corner_ally;

	// Corner closest to enemy base
	Point2D nearest_corner_enemy;

	// Corners adjacent to enemy base corner
	std::vector<Point2D> enemy_adjacent_corners;

	// Retreating flag
	std::unordered_map<const Unit*, bool> battlecruiser_retreating;

	// Retreating location
	std::unordered_map<const Unit*, Point2D> battlecruiser_retreat_location;

	// Moving flag
	std::unordered_map<const Unit*, bool> unit_moving;

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
	bool IsDangerousPosition(const Point2D& pos);

	// Gets the closest safe position for SCVs. (e.g., towards the main base)
	Point2D GetSafePosition();

	// Finds the closest damaged unit for repair.
	const Unit* FindDamagedUnit();

	// Finds the closest damaged structure for repair.
	const Unit* FindDamagedStructure();

	// Returns true if the main base is under attack.
	bool IsMainBaseUnderAttack();

	// Finds the closest enemy unit to a given position.
	const Unit* FindClosestEnemy(const Point2D& pos);

	// Check if the unit has a specific ability.
	bool HasAbility(const Unit* unit, AbilityID ability_id);

	bool TryBuildStructureAtLocation(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type, const Point2D& location);

	Point2D GetRallyPoint();

	const Unit* GetLeastSaturatedBase() const;

	bool IsWorkerUnit(const Unit* unit);

	Point2D GetNearestSafePosition(const Point2D& pos);

	bool IsTrivialUnit(const Unit* unit);

	Point2D GetChokepointPosition();

	bool IsAnyBaseUnderAttack();

	// =========================
	// Ramp
	// =========================

	float cross_product(const Point2D& O, const Point2D& A, const Point2D& B) const;

	Point2D Point2D_mean(const std::vector<Point2D>& points) const;

	std::vector<Point2D> convexHull(std::vector<Point2D>& points) const;

	std::vector<Point2D> circle_intersection(const Point2D& p1, const Point2D& p2, float r) const;

	Point2D towards(const Point2D& p1, const Point2D& p2, float distance) const;

	int height_at(const Point2DI& p) const;

	void find_ramps_build_map(bool isRamp);

	void find_groups(std::vector<Point2D>& points, int minimum_points_per_group, int max_distance_between_points);

	std::vector<Point2D> upper_lower(const std::vector<Point2D>& points, bool up) const;

	Point2D top_bottom_center(const std::vector<Point2D>& points, const bool up) const;

	std::vector<Point2D> upper2_for_ramp_wall(const std::vector<Point2D>& points) const;

	Point2D depot_barrack_in_middle(const std::vector<Point2D>& points, const std::vector<Point2D>& upper2, const bool isdepot) const;

	std::vector<Point2D> corner_depots(const std::vector<Point2D>& points) const;

	void build_supply_depot(const Point2D& location) const; // test

	void find_right_ramp(const Point2D& location);

	bool barracks_can_fit_addon(const Point2D& barrack_point) const;

	Point2D barracks_correct_placement(const std::vector<Point2D>& ramp_points, const std::vector<Point2D>& corner_depots) const;

	void depot_control();
	// =========================
	// Member Variables
	// =========================
	void MoveToEnemy(const Units& marines, const Units& siege_tanks);

	// Count units in combat
	int UnitsInCombat(UNIT_TYPEID unit_type);

	// Calculates the threat level of enemy units
	int CalculateThreatLevel(const Unit* unit);


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
	size_t num_barracks;
	size_t num_factories;
	size_t num_starports;

	// Map information.
	sc2::Point2D start_location;
	sc2::Point2D enemy_start_location;
	sc2::Point2D retreat_location;
	std::vector<sc2::Point2D> enemy_start_locations;
	std::vector<sc2::Point3D> expansion_locations;
	std::vector<sc2::Point2D> structure_locations;
	std::vector<std::vector<sc2::Point2D>> build_map;

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
	std::vector<std::vector<sc2::Point2D>> ramps;
	std::vector<sc2::Point2D> mainBase_depot_points;
	sc2::Point2D mainBase_barrack_point;


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

	// Track if we need to scout the entire map
	bool scout_entire_map = false;

	/*bool chokepoint_blocked = false;*/
	std::vector<sc2::Unit*> ramp_depots = { nullptr, nullptr };
	std::vector<sc2::Unit*> ramp_middle = { nullptr, nullptr };

	// Map of threat levels for specific anti-air units
	const std::unordered_map<sc2::UNIT_TYPEID, int> threat_levels = {
		{sc2::UNIT_TYPEID::TERRAN_MARINE, 1},
		{sc2::UNIT_TYPEID::TERRAN_GHOST, 1},
		{sc2::UNIT_TYPEID::TERRAN_CYCLONE, 2},
		{sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, 2},
		{sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, 2},
		{sc2::UNIT_TYPEID::TERRAN_THOR, 5},
		{sc2::UNIT_TYPEID::TERRAN_MISSILETURRET, 3},
		{sc2::UNIT_TYPEID::PROTOSS_STALKER, 3},
		{sc2::UNIT_TYPEID::PROTOSS_SENTRY, 1},
		{sc2::UNIT_TYPEID::PROTOSS_ARCHON, 3},
		{sc2::UNIT_TYPEID::PROTOSS_PHOENIX, 2},
		{sc2::UNIT_TYPEID::PROTOSS_VOIDRAY, 5},
		{sc2::UNIT_TYPEID::PROTOSS_CARRIER, 3},
		{sc2::UNIT_TYPEID::PROTOSS_TEMPEST, 2},
		{sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, 3},
		{sc2::UNIT_TYPEID::ZERG_QUEEN, 2},
		{sc2::UNIT_TYPEID::ZERG_HYDRALISK, 2},
		{sc2::UNIT_TYPEID::ZERG_RAVAGER, 3},
		{sc2::UNIT_TYPEID::ZERG_MUTALISK, 2},
		{sc2::UNIT_TYPEID::ZERG_CORRUPTOR, 4},
		{sc2::UNIT_TYPEID::ZERG_SPORECRAWLER, 3}
	};


	bool IsFriendlyStructure(const Unit& unit) const {
		switch (unit.unit_type.ToType()) {
		case UNIT_TYPEID::TERRAN_COMMANDCENTER:
			return true;
		case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
			return true;
		case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
			return true;
		case UNIT_TYPEID::TERRAN_BARRACKS:
			return true;
		case UNIT_TYPEID::TERRAN_FACTORY:
			return true;
		case UNIT_TYPEID::TERRAN_STARPORT:
			return true;
		case UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
			return true;
		case UNIT_TYPEID::TERRAN_ARMORY:
			return true;
		case UNIT_TYPEID::TERRAN_FUSIONCORE:
			return true;
		case UNIT_TYPEID::TERRAN_MISSILETURRET:
			return true;
		case UNIT_TYPEID::TERRAN_BUNKER:
			return true;
		case UNIT_TYPEID::TERRAN_TECHLAB:
			return true;
		case UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
			return true;
		case UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
			return true;
		case UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
			return true;
		default:
			return false;
		}
	}

	// Turret types
	std::vector<UNIT_TYPEID> turret_types = {
			UNIT_TYPEID::TERRAN_MISSILETURRET,
			UNIT_TYPEID::ZERG_SPORECRAWLER,
			UNIT_TYPEID::PROTOSS_PHOTONCANNON
	};

	// Worker types
	std::vector<UNIT_TYPEID> worker_types = {
						UNIT_TYPEID::TERRAN_SCV,
						UNIT_TYPEID::TERRAN_MULE,
						UNIT_TYPEID::PROTOSS_PROBE,
						UNIT_TYPEID::ZERG_DRONE
	};

	// Resource units
	std::vector<UNIT_TYPEID> resource_units = {
						UNIT_TYPEID::ZERG_OVERLORD,
						UNIT_TYPEID::TERRAN_SUPPLYDEPOT,
						UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED,
						UNIT_TYPEID::PROTOSS_PYLON
	};

	// Heavy armored units
	std::vector<UNIT_TYPEID> heavy_armor_units = {
		sc2::UNIT_TYPEID::TERRAN_MARAUDER,
		sc2::UNIT_TYPEID::TERRAN_CYCLONE,
		sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
		sc2::UNIT_TYPEID::TERRAN_THOR,
		sc2::UNIT_TYPEID::TERRAN_BUNKER,
		sc2::UNIT_TYPEID::PROTOSS_STALKER,
		sc2::UNIT_TYPEID::PROTOSS_IMMORTAL,
		sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR,
		sc2::UNIT_TYPEID::PROTOSS_COLOSSUS,
		sc2::UNIT_TYPEID::ZERG_ROACH,
		sc2::UNIT_TYPEID::ZERG_ROACHBURROWED,
		sc2::UNIT_TYPEID::ZERG_RAVAGER,
		sc2::UNIT_TYPEID::ZERG_SWARMHOSTMP,
		sc2::UNIT_TYPEID::ZERG_SWARMHOSTBURROWEDMP,
		sc2::UNIT_TYPEID::ZERG_LURKERMP,
		sc2::UNIT_TYPEID::ZERG_LURKERDENMP,
		sc2::UNIT_TYPEID::ZERG_ULTRALISK,
		sc2::UNIT_TYPEID::ZERG_ULTRALISKBURROWED
	};

	// Maps UPGRADE_ID to ABILITY_ID
	ABILITY_ID GetAbilityForUpgrade(UPGRADE_ID upgrade_id) {
		switch (upgrade_id) {
		case UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1:
			return ABILITY_ID::RESEARCH_TERRANVEHICLEANDSHIPPLATINGLEVEL1;
		case UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL2:
			return ABILITY_ID::RESEARCH_TERRANVEHICLEANDSHIPPLATINGLEVEL2;
		case UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL3:
			return ABILITY_ID::RESEARCH_TERRANVEHICLEANDSHIPPLATINGLEVEL3;
		default:
			return ABILITY_ID::INVALID;
		}
	}

	// Set of completed upgrades
	std::set<UpgradeID> completed_upgrades;
};

#endif