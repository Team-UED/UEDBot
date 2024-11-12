//Other than gathering resources,
//    SCVs must
//
//            Retreat from dangerous
//            situations(e.g.,
//                       enemy rushes or harass) to avoid resource loss deaths.
//
//        Repair damaged Battlecruisers during
//        or after engagements
//               .
//
//           Repair structures during enemy attacks to maintain defenses.
//
//           Attack in some urgent situations
//            (e.g., when the enemy is attacking the main base).


#include "BasicSc2Bot.h"

using namespace sc2;

// Main function to control SCVs
void BasicSc2Bot::ControlSCVs() {
	SCVScout();
    RetreatFromDanger();
    RepairUnits();
    RepairStructures();
    SCVAttackEmergency();
}

// SCVs scout the map to find enemy bases
void BasicSc2Bot::SCVScout() {
    if (is_scouting) {
        return;
    }


    
}

// SCVs retreat from dangerous situations (e.g., enemy rushes)
void BasicSc2Bot::RetreatFromDanger() {
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
            if (IsDangerousPosition(unit->pos)) {
                Actions()->UnitCommand(unit, ABILITY_ID::SMART,
                                       GetSafePosition());
            }
        }
    }
}

// SCVs repair damaged Battlecruisers during or after engagements
void BasicSc2Bot::RepairUnits() {
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
            const Unit *target = FindDamagedUnit();
            if (target) {
                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_REPAIR, target);
            }
        }
    }
}

// SCVs repair damaged structures during enemy attacks
void BasicSc2Bot::RepairStructures() {
    for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
            const Unit *target = FindDamagedStructure();
            if (target) {
                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_REPAIR, target);
            }
        }
    }
}

// SCVs attack in urgent situations (e.g., enemy attacking the main base)
void BasicSc2Bot::SCVAttackEmergency() {
    if (IsMainBaseUnderAttack()) {
        for (const auto &unit : Observation()->GetUnits(Unit::Alliance::Self)) {
            if (unit->unit_type == UNIT_TYPEID::TERRAN_SCV) {
                const Unit *target = FindClosestEnemy(unit->pos);
                if (target) {
                    Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, target);
                }
            }
        }
    }
}

