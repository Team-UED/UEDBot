#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageEconomy() {
    TrainSCVs();
    AssignWorkers();
    TryBuildSupplyDepot();
    BuildRefineries();
    BuildExpansion();
    ReassignWorkers();
}

void BasicSc2Bot::TrainSCVs() {
    const ObservationInterface* observation = Observation();

    // Get all Command Centers, Orbital Commands, and Planetary Fortresses
    Units command_centers = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    if (command_centers.empty()) return;

    // Calculate desired SCV count
    size_t desired_scv_count = bases.size() * 22; // 16 for minerals, 6 for gas per base
    if (num_scvs >= desired_scv_count) return;

    // Ensure we have enough supply
    if (observation->GetFoodUsed() >= observation->GetFoodCap() - 1) {
        return; // No point training if we're near supply cap
    }

    // Train SCVs at available Command Centers
    for (const auto& cc : command_centers) {
        bool is_training_scv = false;
        for (const auto& order : cc->orders) {
            if (order.ability_id == ABILITY_ID::TRAIN_SCV) {
                is_training_scv = true;
                break;
            }
        }
        if (is_training_scv) {
            continue;
        }
        if (observation->GetMinerals() >= 50) {
            Actions()->UnitCommand(cc, ABILITY_ID::TRAIN_SCV);
        }
    }
}

bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type) {
    const ObservationInterface* observation = Observation();
    if (observation->GetMinerals() < 100) {
        return false; // Not enough minerals to build
    }

    Units scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    const Unit* builder = nullptr;

    for (const auto& scv : scvs) {
        bool is_constructing = false;
        for (const auto& order : scv->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
                order.ability_id == ABILITY_ID::BUILD_REFINERY ||
                order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
                order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER) {
                is_constructing = true;
                break;
            }
        }
        if (!is_constructing) {
            builder = scv;
            break;
        }
    }

    if (builder) {
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        Point2D build_location(builder->pos.x + rx * 15.0f, builder->pos.y + ry * 15.0f);
        // Check if the location is valid
        if (Query()->Placement(ability_type_for_structure, build_location)) {
            Actions()->UnitCommand(builder, ability_type_for_structure, build_location);
            return true;
        }
    }
    return false;
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
    const ObservationInterface* observation = Observation();
    int32_t supply_used = observation->GetFoodUsed();
    int32_t supply_cap = observation->GetFoodCap();

    // Build a supply depot when supply used reaches a certain threshold
    if (supply_used >= supply_cap - 4) {
        // Check if a supply depot is already under construction
        Units supply_depots_building = observation->GetUnits(Unit::Self, [](const Unit& unit) {
            return unit.unit_type == UNIT_TYPEID::TERRAN_SUPPLYDEPOT && unit.build_progress < 1.0f;
        });

        if (supply_depots_building.empty()) {
            return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT, UNIT_TYPEID::TERRAN_SCV);
        }
    }
    return false;
}

void BasicSc2Bot::AssignWorkers() {
    Units idle_scvs = Observation()->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
        return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && unit.orders.empty();
    });

    Units minerals = Observation()->GetUnits(Unit::Alliance::Neutral, IsMineralPatch());
    Units refineries = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));

    for (const auto& scv : idle_scvs) {
        if (!minerals.empty()) {
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, minerals.front());
        } else {
            for (const auto& refinery : refineries) {
                if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                    Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, refinery);
                    break;
                }
            }
        }
    }
}

void BasicSc2Bot::ReassignWorkers() {
    const ObservationInterface* observation = Observation();

    // Get all refineries and mineral patches
    Units refineries = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
    Units minerals = observation->GetUnits(Unit::Alliance::Neutral, IsMineralPatch());

    // Loop through all refineries to check if we need to reassign workers
    for (const auto& refinery : refineries) {
        if (refinery->assigned_harvesters > refinery->ideal_harvesters) {
            // If there are too many harvesters, reassign the excess
            int excess_harvesters = refinery->assigned_harvesters - refinery->ideal_harvesters;
            for (int i = 0; i < excess_harvesters; ++i) {
                Units scvs = observation->GetUnits(Unit::Alliance::Self, [refinery](const Unit& unit) {
                    return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && unit.orders.size() == 1 && unit.orders.front().target_unit_tag == refinery->tag;
                });
                if (!scvs.empty() && !minerals.empty()) {
                    Actions()->UnitCommand(scvs.front(), ABILITY_ID::HARVEST_GATHER, minerals.front());
                }
            }
        }
    }

    // Loop through all bases to balance worker distribution
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    for (const auto& base : bases) {
        if (base->assigned_harvesters > base->ideal_harvesters) {
            int excess_harvesters = base->assigned_harvesters - base->ideal_harvesters;
            for (int i = 0; i < excess_harvesters; ++i) {
                Units scvs = observation->GetUnits(Unit::Alliance::Self, [base](const Unit& unit) {
                    return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && unit.orders.size() == 1 && unit.orders.front().target_unit_tag == base->tag;
                });
                if (!scvs.empty()) {
                    // Find another base or refinery that needs more workers
                    for (const auto& target_base : bases) {
                        if (target_base->assigned_harvesters < target_base->ideal_harvesters) {
                            Actions()->UnitCommand(scvs.front(), ABILITY_ID::HARVEST_GATHER, target_base);
                            break;
                        }
                    }
                    for (const auto& refinery : refineries) {
                        if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                            Actions()->UnitCommand(scvs.front(), ABILITY_ID::HARVEST_GATHER, refinery);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void BasicSc2Bot::BuildRefineries() {
    Units bases = Observation()->GetUnits(Unit::Alliance::Self, IsTownHall());

    for (const auto& base : bases) {
        Units geysers = Observation()->GetUnits(Unit::Alliance::Neutral, [base](const Unit& unit) {
            return unit.unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER && Distance2D(unit.pos, base->pos) < 10.0f;
        });

        for (const auto& geyser : geysers) {
            Units refineries = Observation()->GetUnits(Unit::Alliance::Self, [geyser](const Unit& unit) {
                return unit.unit_type == UNIT_TYPEID::TERRAN_REFINERY && Distance2D(unit.pos, geyser->pos) < 1.0f;
            });
            if (refineries.empty() && Observation()->GetMinerals() >= 75) {
                Units scvs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
                const Unit* builder = nullptr;

                for (const auto& scv : scvs) {
                    bool is_constructing = false;
                    for (const auto& order : scv->orders) {
                        if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
                            order.ability_id == ABILITY_ID::BUILD_REFINERY ||
                            order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
                            order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER) {
                            is_constructing = true;
                            break;
                        }
                    }
                    if (!is_constructing) {
                        builder = scv;
                        break;
                    }
                }
                if (builder) {
                    if (Query()->Placement(ABILITY_ID::BUILD_REFINERY, geyser->pos)) {
                        Actions()->UnitCommand(builder, ABILITY_ID::BUILD_REFINERY, geyser);
                    }
                }
            }
        }
    }
}

void BasicSc2Bot::BuildExpansion() {
    if (!NeedExpansion()) return;

    Point3D next_expansion = GetNextExpansion();
    Units scvs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    const Unit* builder = nullptr;

    for (const auto& scv : scvs) {
        bool is_constructing = false;
        for (const auto& order : scv->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER ||
                order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
                order.ability_id == ABILITY_ID::BUILD_REFINERY ||
                order.ability_id == ABILITY_ID::BUILD_BARRACKS) {
                is_constructing = true;
                break;
            }
        }
        if (!is_constructing) {
            builder = scv;
            break;
        }
    }

    if (builder) {
        if (Query()->Placement(ABILITY_ID::BUILD_COMMANDCENTER, next_expansion)) {
            Actions()->UnitCommand(builder, ABILITY_ID::BUILD_COMMANDCENTER, next_expansion);
        }
    }
}
