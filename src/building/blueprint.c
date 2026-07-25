#include "blueprint.h"

#include "building/building.h"
#include "building/clone.h"
#include "building/construction_building.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/warehouse.h"
#include "city/finance.h"
#include "city/warning.h"
#include "core/calc.h"
#include "core/string.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/grid.h"
#include "translation/translation.h"

#include <stdio.h>
#include <string.h>

static struct {
    building_blueprint_item items[BUILDING_BLUEPRINT_MAX_ITEMS];
    int count;
    int width;
    int height;
} data;

/** Footprint size in tiles (warehouse is a 3x3 compound despite size=1 props). */
static int item_footprint_size(building_type type)
{
    if (type == BUILDING_WAREHOUSE) {
        return 3;
    }
    return building_properties_for_type(type)->size;
}

/**
 * Number of geometric orientations for stamp rotate.
 * Pure color/style variants (pavilions, roadblocks, etc.) return 0 — leave as-is.
 */
static int item_geometric_rotation_steps(building_type type)
{
    if (building_properties_for_type(type)->rotation_offset) {
        return 2;
    }
    switch (type) {
        case BUILDING_FORT_JAVELIN:
        case BUILDING_FORT_LEGIONARIES:
        case BUILDING_FORT_MOUNTED:
        case BUILDING_FORT_AUXILIA_INFANTRY:
        case BUILDING_FORT_ARCHERS:
        case BUILDING_WAREHOUSE:
            return 4;
        case BUILDING_HIPPODROME:
        case BUILDING_GATEHOUSE:
        case BUILDING_TRIUMPHAL_ARCH:
            return 2;
        default:
            return 0;
    }
}

void building_blueprint_clear(void)
{
    memset(&data, 0, sizeof(data));
}

int building_blueprint_count(void)
{
    return data.count;
}

int building_blueprint_has(void)
{
    return data.count > 0;
}

const building_blueprint_item *building_blueprint_item_at(int index)
{
    if (index < 0 || index >= data.count) {
        return 0;
    }
    return &data.items[index];
}

int building_blueprint_copy_area(int x1, int y1, int x2, int y2)
{
    int map_w = map_grid_width();
    int map_h = map_grid_height();
    int x_min = calc_bound(x1 < x2 ? x1 : x2, 0, map_w - 1);
    int y_min = calc_bound(y1 < y2 ? y1 : y2, 0, map_h - 1);
    int x_max = calc_bound(x1 > x2 ? x1 : x2, 0, map_w - 1);
    int y_max = calc_bound(y1 > y2 ? y1 : y2, 0, map_h - 1);

    building_blueprint_clear();
    data.width = x_max - x_min + 1;
    data.height = y_max - y_min + 1;

    /* Track captured main building ids without a fixed MAX_BUILDINGS */
    unsigned int seen_ids[BUILDING_BLUEPRINT_MAX_ITEMS];
    int seen_count = 0;

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            int building_id = map_building_at(grid_offset);
            if (!building_id) {
                continue;
            }
            building *b = building_main(building_get(building_id));
            if (!b || !b->id) {
                continue;
            }
            int already = 0;
            for (int s = 0; s < seen_count; s++) {
                if (seen_ids[s] == b->id) {
                    already = 1;
                    break;
                }
            }
            if (already) {
                continue;
            }
            if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_CREATED) {
                continue;
            }
            building_type type = building_clone_type_from_building_type(b->type);
            if (type == BUILDING_NONE) {
                continue;
            }
            /* Warehouse tower is not always TL of the 3x3; use compound origin. */
            int origin_x = b->x;
            int origin_y = b->y;
            if (b->type == BUILDING_WAREHOUSE) {
                int main_off = building_warehouse_get_main_grid_offset(b);
                origin_x = map_grid_offset_to_x(main_off);
                origin_y = map_grid_offset_to_y(main_off);
            }
            if (origin_x < x_min || origin_y < y_min || origin_x > x_max || origin_y > y_max) {
                continue;
            }
            if (data.count >= BUILDING_BLUEPRINT_MAX_ITEMS) {
                city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_FULL), NEW_WARNING_SLOT);
                return data.count;
            }
            if (seen_count < BUILDING_BLUEPRINT_MAX_ITEMS) {
                seen_ids[seen_count++] = b->id;
            }
            building_blueprint_item *item = &data.items[data.count++];
            item->dx = origin_x - x_min;
            item->dy = origin_y - y_min;
            item->type = type;
            item->rotation = building_clone_rotation_from_grid_offset(b->grid_offset);
        }
    }

    if (data.count > 0) {
        city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_COPIED), NEW_WARNING_SLOT);
    } else {
        city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_NO_BUILDINGS), NEW_WARNING_SLOT);
    }
    return data.count;
}

int building_blueprint_copy_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    return building_blueprint_copy_area(x, y, x, y);
}

int building_blueprint_width(void)
{
    return data.width;
}

int building_blueprint_height(void)
{
    return data.height;
}

int building_blueprint_rotate_clockwise(void)
{
    if (!data.count) {
        return 0;
    }
    /* Multi-tile TL: (dx, dy) -> (H - dy - S, dx); then swap bbox W/H */
    int old_w = data.width;
    int old_h = data.height;
    for (int i = 0; i < data.count; i++) {
        int s = item_footprint_size(data.items[i].type);
        int dx = data.items[i].dx;
        int dy = data.items[i].dy;
        data.items[i].dx = old_h - dy - s;
        data.items[i].dy = dx;
        /* Only advance geometric orientations, not pure color/style variants */
        int steps = item_geometric_rotation_steps(data.items[i].type);
        if (steps > 0) {
            data.items[i].rotation = (data.items[i].rotation + 1) % steps;
        }
    }
    data.width = old_h;
    data.height = old_w;
    city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_ROTATED), NEW_WARNING_SLOT);
    return 1;
}

int building_blueprint_mirror_horizontal(void)
{
    if (!data.count) {
        return 0;
    }
    for (int i = 0; i < data.count; i++) {
        int s = item_footprint_size(data.items[i].type);
        data.items[i].dx = data.width - s - data.items[i].dx;
    }
    city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_MIRRORED), NEW_WARNING_SLOT);
    return 1;
}

int building_blueprint_paste_at(int x, int y)
{
    if (!data.count) {
        city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_EMPTY), NEW_WARNING_SLOT);
        return 0;
    }
    if (!map_grid_is_inside(x, y, 1)) {
        return 0;
    }

    game_undo_start_build(BUILDING_CLEAR_LAND);
    int placed = 0;
    int cost_total = 0;

    for (int i = 0; i < data.count; i++) {
        const building_blueprint_item *item = &data.items[i];
        int px = x + item->dx;
        int py = y + item->dy;
        int size = item_footprint_size(item->type);
        if (!map_grid_is_inside(px, py, size)) {
            continue;
        }
        int cost = model_get_building(item->type)->cost;
        if (cost > 0 && city_finance_out_of_money()) {
            city_warning_show(WARNING_OUT_OF_MONEY, NEW_WARNING_SLOT);
            break;
        }
        int saved_rot = building_rotation_get_rotation();
        building_rotation_force_set(item->rotation);
        int ok = building_construction_place_building(item->type, px, py, 1);
        building_rotation_force_set(saved_rot);
        if (ok) {
            placed++;
            if (cost > 0) {
                cost_total += cost;
                city_finance_process_construction(cost);
            }
        }
    }

    if (placed) {
        game_undo_finish_build(cost_total);
        char msg[64];
        snprintf(msg, sizeof(msg), (const char *) translation_for(TR_CITY_WARNING_BLUEPRINT_PASTED),
            placed, data.count, data.width, data.height);
        city_warning_show_custom((const uint8_t *) msg, NEW_WARNING_SLOT);
    } else {
        game_undo_disable();
        city_warning_show_custom(translation_for(TR_CITY_WARNING_BLUEPRINT_PASTE_FAILED), NEW_WARNING_SLOT);
    }
    return placed;
}
