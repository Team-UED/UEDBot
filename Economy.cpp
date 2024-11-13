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

    // Use command centers as bases
    size_t desired_scv_count = command_centers.size() * 23; // 16 for minerals, 6 for gas per base + 1 for building
    Units scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SCV));
    if (scvs.size() >= desired_scv_count) return;

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

    // Find all idle SCVs
    for (const auto& scv : idle_scvs) {

        if (scv == scv_scout) {
            continue;
        }

        const Unit* nearest_mineral = nullptr;
        float min_distance = std::numeric_limits<float>::max();

        const Unit* nearest_refinery = nullptr;
        float min_distance_refinery = std::numeric_limits<float>::max(); // Separate distance for refinery

        // Find the nearest refinery needing more workers
        for (const auto& refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                float distance = sc2::Distance2D(scv->pos, refinery->pos);
                if (distance < min_distance_refinery) {
                    min_distance_refinery = distance;
                    nearest_refinery = refinery;
                }
            }
        }

        // Assign the SCV to it
        if (nearest_refinery) {
            Actions()->UnitCommand(scv, ABILITY_ID::HARVEST_GATHER, nearest_refinery);
        } else {
            // Reset min_distance before searching for minerals
            min_distance = std::numeric_limits<float>::max();

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
    
    // Get all resource structures and patches
    Units refineries = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
    Units minerals = observation->GetUnits(Unit::Alliance::Neutral, IsMineralPatch());
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    
    // Create a map of mineral patches to their assigned base
    std::map<const Unit*, const Unit*> mineral_to_base;
    for (const auto& mineral : minerals) {
        float closest_distance = std::numeric_limits<float>::max();
        const Unit* closest_base = nullptr;
        
        for (const auto& base : bases) {
            float distance = Distance2D(mineral->pos, base->pos);
            if (distance < closest_distance) {
                closest_distance = distance;
                closest_base = base;
            }
        }
        
        if (closest_base && closest_distance < 12.0f) {  // Maximum distance for mineral-base association
            mineral_to_base[mineral] = closest_base;
        }
    }
    
    // Track workers that need reassignment
    std::vector<const Unit*> workers_to_reassign;
    
    // Handle excess refinery workers
    for (const auto& refinery : refineries) {
        if (refinery->assigned_harvesters > refinery->ideal_harvesters) {
            int excess_workers = refinery->assigned_harvesters - refinery->ideal_harvesters;
            
            // Find workers currently harvesting from this refinery
            Units gas_workers = observation->GetUnits(Unit::Alliance::Self, 
                [refinery](const Unit& unit) {
                    return unit.unit_type == UNIT_TYPEID::TERRAN_SCV && 
                           !unit.orders.empty() &&
                           unit.orders.front().target_unit_tag == refinery->tag;
                });
            
            // Add excess workers to reassignment list
            for (int i = 0; i < excess_workers && i < gas_workers.size(); ++i) {
                workers_to_reassign.push_back(gas_workers[i]);
            }
        }
    }
    
    // Handle bases with too many workers
    for (const auto& base : bases) {
        if (base->assigned_harvesters > base->ideal_harvesters) {
            int excess_workers = base->assigned_harvesters - base->ideal_harvesters;
            
            // Find mineral patches associated with this base
            std::vector<const Unit*> base_minerals;
            for (const auto& pair : mineral_to_base) {
                if (pair.second == base) {
                    base_minerals.push_back(pair.first);
                }
            }
            
            // Find workers mining from these minerals
            for (const auto& mineral : base_minerals) {
                Units mineral_workers = observation->GetUnits(Unit::Alliance::Self,
                    [mineral](const Unit& unit) {
                        return unit.unit_type == UNIT_TYPEID::TERRAN_SCV &&
                               !unit.orders.empty() &&
                               unit.orders.front().target_unit_tag == mineral->tag;
                    });
                
                // Add excess workers to reassignment list
                for (int i = 0; i < excess_workers && i < mineral_workers.size(); ++i) {
                    workers_to_reassign.push_back(mineral_workers[i]);
                }
                
                if (workers_to_reassign.size() >= excess_workers) {
                    break;
                }
            }
        }
    }
    
    // Reassign workers to understaffed bases or refineries
    for (const auto& worker : workers_to_reassign) {
        // First priority: understaffed refineries that need exactly one more worker
        for (const auto& refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                Actions()->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, refinery);
            }
        }
        
        // Second priority: understaffed bases
        const Unit* target_base = nullptr;
        int largest_worker_deficit = 0;
        
        for (const auto& base : bases) {
            int deficit = base->ideal_harvesters - base->assigned_harvesters;
            if (deficit > largest_worker_deficit) {
                largest_worker_deficit = deficit;
                target_base = base;
            }
        }
        
        if (target_base) {
            // Find closest mineral patch near the target base
            const Unit* target_mineral = nullptr;
            float min_distance = std::numeric_limits<float>::max();
            
            for (const auto& pair : mineral_to_base) {
                if (pair.second == target_base) {
                    float distance = Distance2D(worker->pos, pair.first->pos);
                    if (distance < min_distance) {
                        min_distance = distance;
                        target_mineral = pair.first;
                    }
                }
            }
            
            if (target_mineral) {
                Actions()->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, target_mineral);
                goto worker_reassigned;
            }
        }
        
        // If no understaffed base found, assign to nearest mineral patch
        {
            const Unit* nearest_mineral = nullptr;
            float min_distance = std::numeric_limits<float>::max();
            
            for (const auto& mineral : minerals) {
                if (mineral->mineral_contents > 0) {
                    float distance = Distance2D(worker->pos, mineral->pos);
                    if (distance < min_distance) {
                        min_distance = distance;
                        nearest_mineral = mineral;
                    }
                }
            }
            
            if (nearest_mineral) {
                Actions()->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, nearest_mineral);
            }
        }
        
        worker_reassigned:
        continue;
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