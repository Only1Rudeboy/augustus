#ifndef BUILDING_BLUEPRINT_H
#define BUILDING_BLUEPRINT_H

#include "building/type.h"

/**
 * Multi-building stamp: copy a rectangular area of buildings, paste at cursor.
 * Houses become vacant lots. Only main buildings are stored (no multi-tile parts).
 */

#define BUILDING_BLUEPRINT_MAX_ITEMS 96

typedef struct {
    int dx;
    int dy;
    building_type type;
    int rotation;
} building_blueprint_item;

void building_blueprint_clear(void);

/**
 * Capture buildings in an inclusive map rectangle (any orientation order of corners).
 * @return Number of items stored (0 if none / failed)
 */
int building_blueprint_copy_area(int x1, int y1, int x2, int y2);

/** Capture a single building under the given grid offset into a 1-item blueprint. */
int building_blueprint_copy_at(int grid_offset);

int building_blueprint_count(void);
int building_blueprint_has(void);

const building_blueprint_item *building_blueprint_item_at(int index);

/**
 * Place stamp with top-left of the original selection at (x, y).
 * @return Number of buildings successfully placed
 */
int building_blueprint_paste_at(int x, int y);

/** Rotate stamp 90° clockwise around selection center. Returns 1 if non-empty. */
int building_blueprint_rotate_clockwise(void);

/** Mirror stamp horizontally. Returns 1 if non-empty. */
int building_blueprint_mirror_horizontal(void);

/** Original selection width/height in tiles (after last copy; updates on rotate). */
int building_blueprint_width(void);
int building_blueprint_height(void);

#endif // BUILDING_BLUEPRINT_H
