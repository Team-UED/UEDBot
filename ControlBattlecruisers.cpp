
#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control Battlecruisers
void BasicSc2Bot::ControlBattlecruisers() {
    Jump();
	Target();
	Retreat();
}

void BasicSc2Bot::Jump() {
    for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER && unit->health >= unit->health_max) {
            Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_TACTICALJUMP, enemy_start_location);
        }
    }
}

// target workers first
void BasicSc2Bot::Target() {
	for (const auto& unit : Observation()->GetUnits(Unit::Alliance::Self)) {
		if (unit->unit_type == UNIT_TYPEID::TERRAN_BATTLECRUISER) {
			for (const auto& enemy_unit : Observation()->GetUnits(Unit::Alliance::Enemy)) {
				if (enemy_unit->unit_type == UNIT_TYPEID::TERRAN_SCV || 
					enemy_unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE || 
					enemy_unit->unit_type == UNIT_TYPEID::ZERG_DRONE) {
					Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_unit);
				}
			}
		}
	}
}

void BasicSc2Bot::Retreat() {

}


