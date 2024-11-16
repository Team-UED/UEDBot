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

    // Get all bases
    Units command_centers = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    if (command_centers.empty()) return;

    // Calculate desired SCV count based on ideal harvesters
    size_t desired_scv_count = 0;
    for (const auto& base : command_centers) {
        if (base->build_progress == 1.0f && !base->is_flying) {
            desired_scv_count += base->ideal_harvesters;
        }
    }
    desired_scv_count += 5; // Additional SCVs for building and contingency

    Units scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    if (scvs.size() >= desired_scv_count) return;

    // Ensure we have enough supply
    if (observation->GetFoodUsed() >= observation->GetFoodCap() - 1) {
        return; // Avoid training if we're near supply cap
    }

    // Train SCVs at available Command Centers
    for (const auto& cc : command_centers) {
        if (cc->build_progress < 1.0f || cc->is_flying) {
            continue; // Skip unfinished or flying Command Centers
        }
        bool is_training_scv = false;
        for (const auto& order : cc->orders) {
            if (order.ability_id == ABILITY_ID::TRAIN_SCV) {
                is_training_scv = true;
                break;
            }
        }
        if (!is_training_scv && observation->GetMinerals() >= 50) {
            Actions()->UnitCommand(cc, ABILITY_ID::TRAIN_SCV);
        }
    }
}

void BasicSc2Bot::UseMULE() {
    const ObservationInterface* observation = Observation();

    // Find all Orbital Commands
    Units orbital_commands = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ORBITALCOMMAND));

    // No Orbital Commands found
    if (orbital_commands.empty()) {
        return;
    }

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

    // Max distance from base center
    const float base_radius = 25.0f;

    // Minimum distance from mineral
    const float distance_from_minerals = 10.0f;

    // Find Command Centers
    Units command_centers = observation->GetUnits(Unit::Alliance::Self, [](const Unit& unit) {
        return unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER ||
            unit.unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
            unit.unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS;
    });


    // Find an SCV to build with
    Units scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    const Unit* builder = nullptr;

    for (const auto& scv : scvs) {
        if (scv == scv_scout) {
            continue;
        }

        float min_distance_to_base = std::numeric_limits<float>::max();
        for (const auto& command_center : command_centers) {
            float distance_to_cc = sc2::Distance2D(scv->pos, command_center->pos);
            if (distance_to_cc < min_distance_to_base) {
                min_distance_to_base = distance_to_cc;
            }
        }

        // Check if the SCV is within the base radius
        if (min_distance_to_base > base_radius) {
            continue; 
        }

        bool is_constructing = false;
        for (const auto& order : scv->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
                order.ability_id == ABILITY_ID::BUILD_REFINERY ||
                order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
                order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER ||
                order.ability_id == ABILITY_ID::BUILD_FACTORY ||
                order.ability_id == ABILITY_ID::BUILD_STARPORT ||
                order.ability_id == ABILITY_ID::BUILD_ENGINEERINGBAY ||
                order.ability_id == ABILITY_ID::BUILD_ARMORY ||
                order.ability_id == ABILITY_ID::BUILD_FUSIONCORE ||
                order.ability_id == ABILITY_ID::BUILD_MISSILETURRET ||
                order.ability_id == ABILITY_ID::BUILD_BUNKER
                ) {
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
        // Get main base location
        const Unit* main_base = GetMainBase();
        if (!main_base) {
            return false;
        }

        // Define a build location offset from the main base
        Point2D build_location = main_base->pos + Point2D(10.0f, 0.0f); // Adjust offset as needed

        // Ensure build location is not too close to minerals and not too far from base
        Units mineral_patches = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_MINERALFIELD));

        auto is_valid_build_location = [&](const Point2D& location) {

            if (!main_base) {
                return false;
            }

            // Too far from the base
            float distance_to_base = sc2::Distance2D(location, main_base->pos);
            if (distance_to_base > base_radius) {
                return false; // Too far from the base
            }

            // Too close to minerals
            for (const auto& mineral_patch : mineral_patches) {
                float distance_to_mineral = sc2::Distance2D(location, mineral_patch->pos);
                if (distance_to_mineral < distance_from_minerals) {
                    return false; 
                }
            }
            // Location is valid
            return true; 
            };

        // Check the initial build location
        if (is_valid_build_location(build_location) && Query()->Placement(ability_type_for_structure, build_location)) {
            Actions()->UnitCommand(builder, ability_type_for_structure, build_location);
            return true;
        }
        else {
            // Try to find a nearby valid location
            for (float x = -15.0f; x <= 15.0f; x += 2.0f) {
                for (float y = -15.0f; y <= 15.0f; y += 2.0f) {
                    Point2D test_location = main_base->pos + Point2D(x, y);
                    if (is_valid_build_location(test_location) && Query()->Placement(ability_type_for_structure, test_location)) {
                        Actions()->UnitCommand(builder, ability_type_for_structure, test_location);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
    const ObservationInterface* observation = Observation();
    int32_t supply_used = observation->GetFoodUsed();
    int32_t supply_cap = observation->GetFoodCap();

    // Default surplus
    int32_t supply_surplus = 2;

    // Set surplus by phase
    if (phase == 1) {
        supply_surplus = 2;
    }
    else if (phase == 2) {
        supply_surplus = 6;
    }
    else if (phase == 3) {
        supply_surplus = 8;
    }

    // Build a supply depot when supply used reaches the threshold
    if (supply_used >= supply_cap - supply_surplus) {
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

    Units bases = Observation()->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units refineries = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));

    // Map of base positions to their nearby mineral patches
    std::map<const Unit*, Units> base_minerals_map;
    for (const auto& base : bases) {
        Units nearby_minerals = Observation()->GetUnits(Unit::Alliance::Neutral, [base](const Unit& unit) {
            return IsMineralPatch()(unit) && Distance2D(unit.pos, base->pos) < 10.0f;
        });
        base_minerals_map[base] = nearby_minerals;
    }

    // Assign each idle SCV to the nearest mineral patch associated with our bases
    for (const auto& scv : idle_scvs) {
        if (scv == scv_scout) {
            continue;
        }

        const Unit* nearest_refinery = nullptr;
        float min_distance_refinery = std::numeric_limits<float>::max();

        // Assign to refineries needing workers
        for (const auto& refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                float distance = Distance2D(scv->pos, refinery->pos);
                if (distance < min_distance_refinery) {
                    min_distance_refinery = distance;
                    nearest_refinery = refinery;
                }
            }
        }

        if (nearest_refinery) {
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, nearest_refinery);
            continue;
        }

        // Assign to the nearest mineral patch at our bases
        const Unit* closest_mineral = nullptr;
        float min_distance_mineral = std::numeric_limits<float>::max();

        for (const auto& base_pair : base_minerals_map) {
            for (const auto& mineral : base_pair.second) {
                float distance = Distance2D(scv->pos, mineral->pos);
                if (distance < min_distance_mineral) {
                    min_distance_mineral = distance;
                    closest_mineral = mineral;
                }
            }
        }

        if (closest_mineral) {
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, closest_mineral);
        }
    }
}

void BasicSc2Bot::ReassignWorkers() {
    const ObservationInterface* observation = Observation();

    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units refineries = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));

    // Collect under-saturated and over-saturated bases
    std::vector<const Unit*> under_saturated_bases;
    std::vector<const Unit*> over_saturated_bases;

    for (const auto& base : bases) {
        if (base->build_progress < 1.0f || base->is_flying) {
            continue; // Skip unfinished or flying bases
        }

        int worker_diff = base->ideal_harvesters - base->assigned_harvesters;
        if (worker_diff > 0) {
            under_saturated_bases.push_back(base);
        } else if (worker_diff < 0) {
            over_saturated_bases.push_back(base);
        }
    }

    // Reassign workers from over-saturated to under-saturated bases
    for (const auto& over_base : over_saturated_bases) {
        int excess_workers = over_base->assigned_harvesters - over_base->ideal_harvesters;

        Units workers_at_base = observation->GetUnits(Unit::Alliance::Self, [over_base](const Unit& unit) {
            return unit.unit_type == UNIT_TYPEID::TERRAN_SCV &&
                   !unit.orders.empty() &&
                   Distance2D(unit.pos, over_base->pos) < 10.0f;
        });

        for (int i = 0; i < excess_workers && i < workers_at_base.size(); ++i) {
            const Unit* worker = workers_at_base[i];

            // Assign to the closest under-saturated base
            const Unit* closest_base = nullptr;
            float min_distance = std::numeric_limits<float>::max();

            for (const auto& under_base : under_saturated_bases) {
                float distance = Distance2D(worker->pos, under_base->pos);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_base = under_base;
                }
            }

            if (closest_base) {
                // Assign worker to a mineral patch near the under-saturated base
                Units minerals = observation->GetUnits(Unit::Alliance::Neutral, [closest_base](const Unit& unit) {
                    return IsMineralPatch()(unit) && Distance2D(unit.pos, closest_base->pos) < 10.0f;
                });

                if (!minerals.empty()) {
                    const Unit* closest_mineral = *std::min_element(minerals.begin(), minerals.end(),
                        [worker](const Unit* a, const Unit* b) {
                            return Distance2D(worker->pos, a->pos) < Distance2D(worker->pos, b->pos);
                        });

                    Actions()->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, closest_mineral);
                }
            }
        }
    }

    // Handle over-saturated refineries
    for (const auto& refinery : refineries) {
        int excess_workers = refinery->assigned_harvesters - refinery->ideal_harvesters;

        if (excess_workers > 0) {
            Units gas_workers = observation->GetUnits(Unit::Alliance::Self, [refinery](const Unit& unit) {
                return unit.unit_type == UNIT_TYPEID::TERRAN_SCV &&
                       !unit.orders.empty() &&
                       unit.orders.front().target_unit_tag == refinery->tag;
            });

            for (int i = 0; i < excess_workers && i < gas_workers.size(); ++i) {
                const Unit* worker = gas_workers[i];

                // Assign worker to the closest mineral patch
                Units minerals = observation->GetUnits(Unit::Alliance::Neutral, [worker](const Unit& unit) {
                    return IsMineralPatch()(unit);
                });

                if (!minerals.empty()) {
                    const Unit* closest_mineral = *std::min_element(minerals.begin(), minerals.end(),
                        [worker](const Unit* a, const Unit* b) {
                            return Distance2D(worker->pos, a->pos) < Distance2D(worker->pos, b->pos);
                        });

                    Actions()->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, closest_mineral);
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
    const ObservationInterface* observation = Observation();

    // Check if the first battlecruiser is in production 
	if (!first_battlecruiser) {
		return;
	}

    // Check if we have enough resources to expand
    if (observation->GetMinerals() < 400) {
        return;
    }

    // Check if we need to expand
    if (!NeedExpansion()) {
        return;
    }

    // Check if a Command Center is already being built
    Units command_centers_building = observation->GetUnits(Unit::Self, [](const Unit& unit) {
        return (unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER ||
                unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING ||
                unit.unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
                unit.unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING ||
                unit.unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS) &&
               unit.build_progress < 1.0f;
    });

    if (!command_centers_building.empty()) {
        return; // An expansion is already being built
    }

    Point3D next_expansion = GetNextExpansion();
    if (next_expansion == Point3D(0.0f, 0.0f, 0.0f)) {
        return;
    }

    // Check if the expansion location is safe
    if (IsDangerousPosition(next_expansion)) {
        return; // Don't expand to a location that's under threat
    }

    Units scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    const Unit* builder = nullptr;

    // Find an idle SCV to build the expansion
    for (const auto& scv : scvs) {
        if (scv->orders.empty()) {
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
