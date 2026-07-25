#include "depot.h"

#include "assets/assets.h"
#include "building/granary.h"
#include "building/industry.h"
#include "building/storage.h"
#include "building/warehouse.h"
#include "city/health.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/resource.h"
#include "map/road_access.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/routing_terrain.h"

#define DEPOT_CART_PUSHER_SPEED 1

#define DEPOT_CART_PUSHER_FOOD_CAPACITY 16
#define DEPOT_CART_PUSHER_OTHER_CAPACITY 4

#define DEPOT_CART_REROUTE_DELAY 10
#define DEPOT_CART_LOAD_OFFLOAD_DELAY 10

static const int CART_OFFSETS_X[8] = { 24, 34, 29,  7, -15, -20, -13,  6 };
static const int CART_OFFSETS_Y[8] = { -5,  6, 17, 40,  15,   7,  -3, -6 };

static int cartpusher_carries_food(figure *f)
{
    return resource_is_food(f->resource_id);
}

static void set_cart_graphic(figure *f)
{
    int carried = f->loads_sold_or_carrying;
    if (carried == 0 || f->resource_id == RESOURCE_NONE) {
        f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART);
    } else if (carried == 1) {
        f->cart_image_id = resource_get_data(f->resource_id)->image.cart.single_load;
    } else if (cartpusher_carries_food(f) && carried >= 8) {
        f->cart_image_id = resource_get_data(f->resource_id)->image.cart.eight_loads;
    } else {
        f->cart_image_id = resource_get_data(f->resource_id)->image.cart.multiple_loads;
    }
}

static void set_cart_offset(figure *f, int direction)
{
    f->x_offset_cart = CART_OFFSETS_X[direction];
    f->y_offset_cart = CART_OFFSETS_Y[direction];

    if (f->loads_sold_or_carrying >= 8) {
        f->y_offset_cart -= 40;
    } else if (direction == 3) {
        f->y_offset_cart -= 20;
    }
}

static void update_image(figure *f)
{
    int dir = figure_image_normalize_direction(
        f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->image_id = assets_lookup_image_id(ASSET_OX) + 1 + figure_image_corpse_offset(f);
        f->cart_image_id = 0;
    } else {
        f->image_id = assets_lookup_image_id(ASSET_OX) + 9 + dir * 12 + f->image_offset;
    }
    if (f->cart_image_id) {
        dir = (dir + 4) % 8;
        f->cart_image_id += dir;
        set_cart_offset(f, dir);
    }
}

static int get_storage_road_access(building *b, map_point *point)
{
    if (b->type == BUILDING_GRANARY) {
        map_point_store_result(b->x + 1, b->y + 1, point);
        return 1;
    } else if (b->type == BUILDING_WAREHOUSE) {
        if (b->has_road_access == 1) {
            map_point_store_result(b->x, b->y, point);
            return 1;
        } else {
            return map_has_road_access_warehouse(b->x, b->y, point);
        }
    } else {
        point->x = b->road_access_x;
        point->y = b->road_access_y;
        return b->has_road_access;
    }
}

static int is_order_condition_satisfied(const order *current_order)
{
    if (!current_order->src_storage_id || !current_order->dst_storage_id || !current_order->resource_type) {
        return 0;
    }

    if (current_order->condition.condition_type == ORDER_CONDITION_NEVER) {
        return 0;
    }
    building *src = building_get(current_order->src_storage_id);
    building *dst = building_get(current_order->dst_storage_id);
    if (!building_is_active(src) || !building_is_active(dst)) {
        return 0;
    }
    int src_amount = src->type == BUILDING_GRANARY ?
        building_granary_get_amount(src, current_order->resource_type) :
        building_warehouse_get_amount(src, current_order->resource_type);

    int dst_amount = dst->type == BUILDING_GRANARY ?
        building_granary_get_amount(dst, current_order->resource_type) :
        building_warehouse_get_amount(dst, current_order->resource_type);
    if (src_amount == 0) {
        return 0;
    }

    switch (current_order->condition.condition_type) {
        case ORDER_CONDITION_SOURCE_HAS_MORE_THAN:
            return src_amount >= current_order->condition.threshold;
        case ORDER_CONDITION_DESTINATION_HAS_LESS_THAN:
            return dst_amount < current_order->condition.threshold;
        default:
            return 1;
    }
}

static int storage_remove_resource(building *b, int resource, int amount)
{
    if (b->type == BUILDING_GRANARY) {
        return building_granary_try_remove_resource(b, resource, amount);
    } else if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_try_remove_resource(b, resource, amount);
    } else {
        return 0;
    }
}

static int storage_add_resource(building *b, int resource, int amount)
{
    while (amount > 0) {
        int unload_amount = (amount > 2) ? 2 : 1; // unload in 2s if possible
        unsigned char added_amount = building_storage_try_add_resource(b, resource, unload_amount, 0);
        if (added_amount <= 0) {
            return amount; // not enough space
        }
        amount -= added_amount;
    }
    return amount;
}

static void clear_destination(figure *f)
{
    figure_route_remove(f);
    f->destination_building_id = 0;
    f->destination_x = 0;
    f->destination_y = 0;
}

/* Returns 1 if destination was set. Refuses dead/missing buildings (no silent heading). */
static int set_destination(figure *f, unsigned int building_id, int action_state)
{
    if (!building_id) {
        return 0;
    }
    building *dest = building_get(building_id);
    if (!dest || dest->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    map_point road_access;
    get_storage_road_access(dest, &road_access);
    f->destination_building_id = building_id;
    f->destination_x = road_access.x;
    f->destination_y = road_access.y;
    f->action_state = action_state;
    figure_route_remove(f);
    set_cart_graphic(f);
    return 1;
}

static int is_storage_valid(building *b, int resource)
{
    return b && b->state == BUILDING_STATE_IN_USE &&
        building_storage_get_state(b, resource, 0) != BUILDING_STORAGE_STATE_NOT_ACCEPTING;
}

/* True when the figure's current destination building is usable. */
static int current_destination_in_use(const figure *f)
{
    if (!f->destination_building_id) {
        return 0;
    }
    building *dest = building_get(f->destination_building_id);
    return dest && dest->state == BUILDING_STATE_IN_USE;
}

/* Re-route remaining cargo without dropping it: prefer source, then order dest. */
static void reroute_with_remaining_cargo(figure *f, building *depot)
{
    unsigned int src_id = depot->data.depot.current_order.src_storage_id;
    unsigned int dst_id = depot->data.depot.current_order.dst_storage_id;
    building *src = building_get(src_id);
    building *dst = building_get(dst_id);

    clear_destination(f);
    if (src_id && is_storage_valid(src, f->resource_id) &&
        set_destination(f, src_id, FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE)) {
        return;
    }
    if (dst_id && is_storage_valid(dst, f->resource_id) &&
        set_destination(f, dst_id, FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION)) {
        return;
    }
    /* No valid sink — hold cargo and wait for order/storage update. Never die with cargo. */
    f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
    set_cart_graphic(f);
}

static void try_reroute_order_dst(figure *f, building *b)
{
    if (f->action_state != FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION &&
        f->action_state != FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION) {
        return;
    }
    if (f->loads_sold_or_carrying <= 0 || f->resource_id == RESOURCE_NONE) {
        return;
    }
    unsigned int new_dst_id = b->data.depot.current_order.dst_storage_id;
    // If standing at destination and destination didn't change, skip reroute
    if (f->action_state == FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION &&
        (new_dst_id == f->destination_building_id || new_dst_id == 0)) {
        return;
    }
    building *new_dst = building_get(new_dst_id);
    if (is_storage_valid(new_dst, f->resource_id) &&
        set_destination(f, new_dst_id, FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION)) {
        return;
    }
    /* Order dest dead/rejected — clear and re-route remaining cargo. */
    if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
        reroute_with_remaining_cargo(f, b);
    } else {
        clear_destination(f);
        f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
    }
}

static void try_reroute_order_src(figure *f, building *depot)
{
    if (f->action_state != FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE &&
        f->action_state != FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE &&
        f->action_state != FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE) {
        return;
    }

    unsigned int new_src_id = depot->data.depot.current_order.src_storage_id;
    building *new_src = building_get(new_src_id);

    // if source has changed, reroute cart
    if (f->destination_building_id != new_src_id && new_src_id != 0) {
        if (is_storage_valid(new_src, depot->data.depot.current_order.resource_type)) {
            set_destination(f, new_src_id, FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE);
            return;
        }
    }

    if (!is_storage_valid(new_src, depot->data.depot.current_order.resource_type)) {
        // if new source becomes invalid and there is cargo — re-find sink (never home with cargo)
        if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
            reroute_with_remaining_cargo(f, depot);
            return;
        }
        clear_destination(f);
        f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
    }
}

static void figure_cart_unload_or_return(figure *f, building *b)
{
    // if there is no cargo return to the depot
    if (f->loads_sold_or_carrying <= 0 || f->resource_id == RESOURCE_NONE) {
        f->loads_sold_or_carrying = 0;
        f->resource_id = RESOURCE_NONE;
        set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
        return;
    }
    int src_id = b->data.depot.current_order.src_storage_id;
    int dst_id = b->data.depot.current_order.dst_storage_id;
    building *src = building_get(src_id);
    building *dst = building_get(dst_id);

    // determine where to unload: if we are at the source - unload to the source, otherwise to the destination
    int at_source = (f->action_state == FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE);
    building *target = at_source ? src : dst;

    if (!target || !is_storage_valid(target, f->resource_id)) {
        /* Dead / non-accepting sink: keep cargo, clear dest, re-find. */
        if (at_source) {
            /* Returning cargo but source is gone — try dest, else hold (do not return home with cargo). */
            clear_destination(f);
            if (dst_id && is_storage_valid(dst, f->resource_id)) {
                set_destination(f, dst_id, FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION);
            } else {
                f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                set_cart_graphic(f);
            }
        } else {
            /* Destination dead/rejected — return remaining cargo to source if possible. */
            reroute_with_remaining_cargo(f, b);
        }
        return;
    }
    // unload (returns amount that did not fit — never silently discard cargo)
    int before = f->loads_sold_or_carrying;
    f->loads_sold_or_carrying = storage_add_resource(target, f->resource_id, f->loads_sold_or_carrying);

    // If everything is unloaded - return to the depot
    if (f->loads_sold_or_carrying == 0) {
        city_health_dispatch_sickness(f);
        f->resource_id = RESOURCE_NONE;
        set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
    } else {
        /* Partial delivery or rejection: keep remaining cargo and re-find a sink. */
        if (f->loads_sold_or_carrying < before) {
            city_health_dispatch_sickness(f);
        }
        set_cart_graphic(f);
        if (at_source) {
            /*
             * Source full while returning leftover goods — wait and retry here.
             * Avoid bouncing dest↔source forever when both are full.
             */
        } else {
            /* Failed at destination: clear dest and try returning remainder to source. */
            reroute_with_remaining_cargo(f, b);
        }
    }
}

void figure_depot_cartpusher_action(figure *f)
{
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    int speed_factor = DEPOT_CART_PUSHER_SPEED;
    int percentage_speed = 0;
    if (config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD)) {
        f->terrain_usage = TERRAIN_USAGE_ANY;
    } else {
        f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }
    building *b = building_get(f->building_id);
    if (!b || b->type != BUILDING_DEPOT || b->state != BUILDING_STATE_IN_USE) {
        f->state = FIGURE_STATE_DEAD;
        update_image(f);
        return;
    }
    int is_linked = 0;
    for (int i = 0; i < 3; i++) {
        if (b->data.distribution.cartpusher_ids[i] == f->id) {
            is_linked = 1;
        }
    }
    if (!is_linked) {
        f->state = FIGURE_STATE_DEAD;
        update_image(f);
        return;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL:
        {
            set_cart_graphic(f);
            if (!map_routing_citizen_is_passable(f->grid_offset)) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (is_order_condition_satisfied(&b->data.depot.current_order)) {
                set_destination(f, b->data.depot.current_order.src_storage_id,
                    FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE);
                f->wait_ticks = DEPOT_CART_REROUTE_DELAY + 1;
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
            f->image_offset = 0;
            break;
        }

        case FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE:
        case FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION:
        case FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE:
        {
            set_cart_graphic(f);
            /* Destination deleted / not IN_USE mid-trip: clear dest and re-route (keep cargo). */
            if (!current_destination_in_use(f)) {
                clear_destination(f);
                f->wait_ticks = 0;
                if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
                    reroute_with_remaining_cargo(f, b);
                } else {
                    f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                }
                break;
            }
            if (f->wait_ticks > DEPOT_CART_REROUTE_DELAY) {
                figure_movement_move_ticks_with_percentage(f, speed_factor, percentage_speed);

                try_reroute_order_src(f, b);

                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    if (f->action_state == FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE ||
                        f->action_state == FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE) {
                        f->action_state = FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE;
                    } else {
                        f->action_state = FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION;
                    }
                    f->wait_ticks = 0;
                } else if (f->direction == DIR_FIGURE_LOST) {
                    clear_destination(f);
                    f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                    f->wait_ticks = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE) {
                    figure_route_remove(f);
                    f->wait_ticks = 0;
                }
            } else {
                f->wait_ticks++;
            }
            if (f->action_state == FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION) {
                try_reroute_order_dst(f, b);
            }
            break;
        }

        case FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE:
        {
            set_cart_graphic(f);
            f->wait_ticks++;

            try_reroute_order_src(f, b);

            if (f->wait_ticks > DEPOT_CART_LOAD_OFFLOAD_DELAY) {
                building *src = building_get(b->data.depot.current_order.src_storage_id);
                // If the cart has remaining cargo, unload it using the common function
                if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
                    figure_cart_unload_or_return(f, b);
                    f->wait_ticks = 0;
                    break;
                }

                // Depot cartpusher waits if not enough goods
                int src_amount = src->type == BUILDING_GRANARY ?
                    building_granary_get_amount(src, b->data.depot.current_order.resource_type) :
                    building_warehouse_get_amount(src, b->data.depot.current_order.resource_type);
                int condition_type = b->data.depot.current_order.condition.condition_type;
                int threshold = b->data.depot.current_order.condition.threshold;
                if ((condition_type == ORDER_CONDITION_SOURCE_HAS_MORE_THAN && src_amount < threshold) ||
                    (condition_type == ORDER_CONDITION_DESTINATION_HAS_LESS_THAN && src_amount < 4)) {
                    break; // wait at source
                }

                // loading TODO upgradable?
                int capacity = resource_is_food(b->data.depot.current_order.resource_type) ?
                    DEPOT_CART_PUSHER_FOOD_CAPACITY : DEPOT_CART_PUSHER_OTHER_CAPACITY;
                int amount_loaded = storage_remove_resource(src, b->data.depot.current_order.resource_type, capacity);
                if (amount_loaded > 0) {
                    city_health_dispatch_sickness(f);
                    f->resource_id = b->data.depot.current_order.resource_type;
                    f->loads_sold_or_carrying = amount_loaded;

                    if (!set_destination(f, b->data.depot.current_order.dst_storage_id,
                                                FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION)) {
                        /* Dest dead/missing after taking cargo — re-find without dropping goods. */
                        reroute_with_remaining_cargo(f, b);
                    }
                }
                f->wait_ticks = 0;
            }
            break;
        }

        case FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION:
            set_cart_graphic(f);
            /* Dest deleted while waiting to unload: re-route remaining cargo. */
            if (!current_destination_in_use(f)) {
                clear_destination(f);
                f->wait_ticks = 0;
                if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
                    reroute_with_remaining_cargo(f, b);
                } else {
                    f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                }
                break;
            }
            f->wait_ticks++;
            if (f->wait_ticks > DEPOT_CART_LOAD_OFFLOAD_DELAY) {
                figure_cart_unload_or_return(f, b);
                f->wait_ticks = 0;
            }
            try_reroute_order_dst(f, b);
            break;

        case FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING:
            set_cart_graphic(f);
            figure_movement_move_ticks_with_percentage(f, speed_factor, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_LOST) {
                //If source and destination are lost, wait for new instructions
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
                f->wait_ticks = 0;
            }
            break;

        case FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER:
        {
            if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
                /* Prefer putting cargo back at source; else try dest; else hold (no cargo drop). */
                reroute_with_remaining_cargo(f, b);
            } else {
                f->loads_sold_or_carrying = 0;
                f->resource_id = RESOURCE_NONE;
                set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
            }
            set_cart_graphic(f);
            break;
        }
    }
    update_image(f);
}

void figure_depot_recall(figure *f)
{
    f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
}
