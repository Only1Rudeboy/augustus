#ifndef BUILDING_HOUSE_EVOLUTION_H
#define BUILDING_HOUSE_EVOLUTION_H

#include "building/building.h"

/**
 * Evolves/devolves houses if appropriate, and consumes pottery/furniture/oil/wine
 */
void building_house_process_evolve_and_consume_goods(void);

/**
 * Determine the text to show for evolution of a house, stored in house->evolve_text_id
 * @param house House to determine text for
 * @param worst_desirability_building The ID of the building with worst contribution to desirability
 */
void building_house_determine_evolve_text(building *house, int worst_desirability_building);

/**
 * Determine building with worst contribution to desirability
 * @param house House to determine worst building for
 * @return Worst desirability building ID
 */
building_type building_house_determine_worst_desirability_building_type(const building *house);

/** Compact multi-reason checklist for the next house upgrade (read-only UI). */
typedef struct {
    int can_evolve;
    int desirability_current;
    int desirability_needed;
    int missing_desirability;
    int missing_water;
    int missing_entertainment;
    int entertainment_have;
    int entertainment_need;
    int missing_education;
    int education_have;
    int education_need;
    int missing_religion;
    int gods_have;
    int gods_need;
    int missing_barber;
    int missing_bathhouse;
    int missing_health;
    int health_have;
    int health_need;
    int missing_food;
    int food_have;
    int food_need;
    int missing_pottery;
    int pottery_have;
    int pottery_need;
    int missing_oil;
    int oil_have;
    int oil_need;
    int missing_furniture;
    int furniture_have;
    int furniture_need;
    int missing_wine;
    int wine_have;
    int wine_need;
    int missing_second_wine;
    int missing_space;
    int is_max_level;
    int has_plague;
} house_upgrade_diagnose;

/**
 * Collect all blockers preventing upgrade to the next house level.
 * Does not mutate game state. Uses the same rules as evolution.
 */
void building_house_diagnose_upgrade(const building *house, house_upgrade_diagnose *out);

#endif // BUILDING_HOUSE_EVOLUTION_H
