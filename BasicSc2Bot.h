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

using namespace sc2;

class BasicSc2Bot : public sc2::Agent {
public:
	virtual void OnGameStart();
	virtual void OnGameEnd();
	virtual void OnStep();

    void OnUnitIdle(const Unit *unit);
    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV);
    bool TryBuildSupplyDepot();
    bool TryBuildBarracks();
    size_t CountUnitType(UNIT_TYPEID unit_type);
    void Scout(const Unit* unit);

    const Unit* FindNearestMineralPatch(const Point2D& start);

private:
};

#endif