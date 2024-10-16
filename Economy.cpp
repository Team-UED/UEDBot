#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::ManageEconomy() {
    TrainSCVs();
    AssignWorkers();
    TryBuildSupplyDepot();
    BuildRefineries();
    
}

void BasicSc2Bot::TrainSCVs() {
    const ObservationInterface* observation = Observation();

    // Get all types of Command Centers
    Units command_centers = observation->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
        return unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER ||
               unit.unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
               unit.unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS;
    });
    if (command_centers.empty()) return;

    // Calculate desired SCV count
    size_t desired_scv = bases.size() * 16 + bases.size() * 6; // 16 for minerals, 6 for gas per base
    if (num_scvs >= desired_scv) return;

    // Ensure we have enough supply
    if (observation->GetFoodUsed() >= observation->GetFoodCap() - 1) {
        return; // We can't train more SCVs if we're at or near supply cap
    }

    // Loop through each Command Center
    for (const auto& cc : command_centers) {
        // Check if Command Center is able to train SCVs
        bool is_training_scv = false;
        for (const auto& order : cc->orders) {
            if (order.ability_id == ABILITY_ID::TRAIN_SCV) {
                is_training_scv = true;
                break;
            }
        }
        // If not training an SCV, and can afford one, train it
        if (!is_training_scv && observation->GetMinerals() >= 50) {
            Actions()->UnitCommand(cc, ABILITY_ID::TRAIN_SCV);
        }
    }
}


bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type) {
    // Attempts to build a structure with an available SCV.
    const ObservationInterface* observation = Observation();

    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));

    for (const auto& unit : units) {
        // Use the first SCV that is not currently constructing.
        bool is_constructing = false;
        for (const auto& order : unit->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
                order.ability_id == ABILITY_ID::BUILD_REFINERY ||
                order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
                // Add other building abilities as needed
                false) {
                is_constructing = true;
                break;
            }
        }
        if (!is_constructing) {
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
    // Try to build a supply depot if needed.
    const ObservationInterface* observation = Observation();

    int32_t supply_used = observation->GetFoodUsed();
    int32_t supply_cap = observation->GetFoodCap();

    // Build a supply depot when supply used reaches a certain percentage of supply cap.
    if (supply_used >= supply_cap - 4) {
        // Check if a supply depot is already under construction.
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
    // Assign idle SCVs to minerals or gas
    Units idle_scvs = Observation()->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
        return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && unit.orders.empty();
    });

    Units minerals = Observation()->GetUnits(Unit::Alliance::Neutral, IsMineralPatch());
    Units refineries = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));

    for (const auto& scv : idle_scvs) {
        // Assign to minerals first if available
        if (!minerals.empty()) {
            const Unit* mineral = minerals.front();
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, mineral);
        }
        // Otherwise assign to gas if available
        else if (!refineries.empty()) {
            for (const auto& refinery : refineries) {
                if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                    Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, refinery);
                    break;
                }
            }
        }
    }
}

void BasicSc2Bot::BuildRefineries() {
    const ObservationInterface* observation = Observation();

    // Get all Command Centers, Orbital Commands, and Planetary Fortresses
    Units bases = observation->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
        return unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER ||
               unit.unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
               unit.unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS;
    });

    // For each base, attempt to build refineries on nearby vespene geysers
    for (const auto& base : bases) {
        // Create a filter that checks for vespene geysers within 10 units of the base
        auto geyser_filter = [this, base](const Unit& unit) {
            return unit.unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER &&
                   Distance2D(unit.pos, base->pos) < 10.0f;
        };

        // Get vespene geysers near the base
        Units geysers = observation->GetUnits(Unit::Alliance::Neutral, geyser_filter);

        for (const auto& geyser : geysers) {
            // Check if a refinery already exists on the geyser
            bool has_refinery = false;

            // Get units at the geyser's position
            Units units_on_geyser = observation->GetUnits(Unit::Alliance::Self, [geyser](const Unit& unit) {
                return unit.unit_type == UNIT_TYPEID::TERRAN_REFINERY &&
                       Distance2D(unit.pos, geyser->pos) < 1.0f;
            });

            // Check if any refineries are present or under construction on this geyser
            if (!units_on_geyser.empty()) {
                has_refinery = true;
            }

            // If no refinery exists, try to build one
            if (!has_refinery) {
                TryBuildRefinery(geyser);
            }
        }
    }
}


void BasicSc2Bot::TryBuildRefinery(const Unit* geyser) {
    const ObservationInterface* observation = Observation();

    // Check if we have enough minerals to build a refinery (75 minerals)
    if (observation->GetMinerals() < 75) {
        return;
    }

    // Find an available SCV to build the refinery
    const Unit* scv = nullptr;
    Units scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));

    for (const auto& unit : scvs) {
        // Use the first SCV that is not currently constructing
        bool is_constructing = false;
        for (const auto& order : unit->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
                order.ability_id == ABILITY_ID::BUILD_REFINERY ||
                order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
                // Add other building abilities as needed
                false) {
                is_constructing = true;
                break;
            }
        }
        if (!is_constructing) {
            scv = unit;
            break;
        }
    }

    if (scv) {
        // Command the SCV to build a refinery on the geyser
        Actions()->UnitCommand(scv, ABILITY_ID::BUILD_REFINERY, geyser);
    }
}
