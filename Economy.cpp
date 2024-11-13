#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageEconomy() {
    TrainSCVs();
    AssignWorkers();
    TryBuildSupplyDepot();
    BuildRefineries();
    BuildExpansion();
    ReassignWorkers();
	UseMULE();
}

void BasicSc2Bot::TrainSCVs() {
    const ObservationInterface* observation = Observation();

    // Get all Command Centers, Orbital Commands, and Planetary Fortresses
    Units command_centers = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    if (command_centers.empty()) return;

    // Calculate desired SCV count
    size_t desired_scv_count = bases.size() * 23; // 16 for minerals, 6 for gas per base + 1 for building
    Units scvs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    if (static_cast<int>(scvs.size()) >= desired_scv_count) return;

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

void BasicSc2Bot::UseMULE() {
    const ObservationInterface* observation = Observation();

    // Find all Orbital Commands
    Units orbital_commands = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ORBITALCOMMAND));

    // Loop Orbital Command to check if it has enough energy
    for (const auto& orbital : orbital_commands) {
        if (orbital->energy >= 50) { 
            // Find the nearest mineral patch to the Orbital Command
            Units mineral_patches = observation->GetUnits(Unit::Alliance::Neutral, IsMineralPatch());
            const Unit* closest_mineral = nullptr;
            float min_distance = std::numeric_limits<float>::max();

            for (const auto& mineral : mineral_patches) {
                float distance = sc2::Distance2D(orbital->pos, mineral->pos);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_mineral = mineral;
                }
            }

            // use MULE on nearest mineral patch
            if (closest_mineral) {
                Actions()->UnitCommand(orbital, ABILITY_ID::EFFECT_CALLDOWNMULE, closest_mineral);
                return;
            }
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

        if (scv == scv_scout) {
            continue;
        }

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
    if (supply_used >= supply_cap - (phase * 3)) {
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

    // Find all idle scvs
    for (const auto& scv : idle_scvs) {

        if (scv == scv_scout) {
            continue;
        }

        const Unit* nearest_mineral = nullptr;
        float min_distance = std::numeric_limits<float>::max();

        const Unit* nearest_refinery = nullptr;
        min_distance = std::numeric_limits<float>::max();


        // Find the nearest refinery needing more workers
        for (const auto& refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                float distance = sc2::Distance2D(scv->pos, refinery->pos);
                if (distance < min_distance) {
                    min_distance = distance;
                    nearest_refinery = refinery;
                }
            }
        }
        // Assign the SCV to it
        if (nearest_refinery) {
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, nearest_refinery);
        }
        else {
            // Find the nearest mineral patch
            for (const auto& mineral : minerals) {
                float distance = sc2::Distance2D(scv->pos, mineral->pos);
                if (distance < min_distance) {
                    min_distance = distance;
                    nearest_mineral = mineral;
                }
            }
            // Assign the SCV to it
            if (nearest_mineral) {
                Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, nearest_mineral);
            }
        }
    }
}

void BasicSc2Bot::ReassignWorkers() {
    const ObservationInterface* observation = Observation();

    // Get Minerals and refineries
    Units refineries = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
    Units minerals = observation->GetUnits(Unit::Alliance::Neutral, IsMineralPatch());

	// Check if any refineries have too many workers
    for (const auto& refinery : refineries) {
        if (refinery->assigned_harvesters > refinery->ideal_harvesters) {
            int excess_harvesters = refinery->assigned_harvesters - refinery->ideal_harvesters;
            for (int i = 0; i < excess_harvesters; ++i) {
                Units scvs = observation->GetUnits(Unit::Alliance::Self, [refinery](const Unit& unit) {
                    return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && 
                        unit.orders.size() == 1 && unit.orders.front().target_unit_tag == refinery->tag;
                    });

				// Assign excess scvs to nearest minerals
                if (!scvs.empty()) {
                    const Unit* nearest_mineral = nullptr;
                    float min_distance = std::numeric_limits<float>::max();
                    for (const auto& mineral : minerals) {
                        float distance = sc2::Distance2D(scvs.front()->pos, mineral->pos);
                        if (distance < min_distance) {
                            min_distance = distance;
                            nearest_mineral = mineral;
                        }
                    }
                    if (nearest_mineral) {
                        Actions()->UnitCommand(scvs.front(), ABILITY_ID::HARVEST_GATHER, nearest_mineral);
                    }
                }
            }
        }
    }

	// Check if any bases have too many workers (This should be done after expanding base)
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
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
