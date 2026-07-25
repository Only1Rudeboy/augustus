#include "supplier.h"

#include "assets/assets.h"
#include "building/building.h"
#include "building/caravanserai.h"
#include "building/distribution.h"
#include "building/granary.h"
#include "building/highway_station.h"
#include "building/lighthouse.h"
#include "building/market.h"
#include "building/mess_hall.h"
#include "building/storage.h"
#include "building/tavern.h"
#include "building/temple.h"
#include "building/warehouse.h"
#include "city/data_private.h"
#include "city/resource.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figuretype/wall.h"
#include "game/resource.h"
#include "map/data.h"
#include "map/road_access.h"
#include "map/road_network.h"

int figure_supplier_max_stocked_mess_hall_adjusted(void)
{
    int max_stock;
    if (city_data.military.total_legions < 10) {
        max_stock = MAX_FOOD_STOCKED_MESS_HALL;
    } else if (city_data.military.total_legions == 10) {
        max_stock = MAX_FOOD_STOCKED_MESS_HALL * 1.5f; //increase by 50% if max legions
    } else {   //cheat code activated
        max_stock = MAX_FOOD_STOCKED_MESS_HALL * 2; // double the possible stock
    }
    return max_stock;
}

int figure_supplier_create_delivery_boy(int leader_id, int first_figure_id, int type)
{
    figure *f = figure_get(first_figure_id);
    figure *boy = figure_create(type, f->x, f->y, 0);
    f = figure_get(first_figure_id);
    boy->leading_figure_id = leader_id;
    boy->collecting_item_id = f->collecting_item_id;
    boy->loads_sold_or_carrying = 1; // for consistency
    // deliver to destination instead of origin
    if (f->action_state == FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED) {
        boy->building_id = f->destination_building_id;
    } else {
        boy->building_id = f->building_id;
    }
    return boy->id;
}

static int take_food_from_granary(figure *f, int market_id, int granary_id)
{
    resource_type resource = f->collecting_item_id;

    if (!resource_is_food(resource)) {
        return 0;
    }
    building *granary = building_get(granary_id);
    building *market = building_get(market_id);

    int market_units = market->resources[resource];
    int max_units = 0;
    int granary_loads_stored = building_granary_count_available_resource(granary, resource, 1);
    int granary_loads_take = 0;

    if (market->type == BUILDING_MESS_HALL) {
        max_units = figure_supplier_max_stocked_mess_hall_adjusted() - market_units;
    } else if (market->type == BUILDING_CARAVANSERAI) {
        max_units = MAX_FOOD_STOCKED_CARAVANSERAI - market_units;
    } else {
        max_units = MAX_FOOD_STOCKED_MARKET - market_units;
    }
    if (granary_loads_stored > (max_units / RESOURCE_ONE_LOAD)) {
        granary_loads_take = (max_units / RESOURCE_ONE_LOAD);
    } else {
        granary_loads_take = granary_loads_stored;
    }
    if (!granary_loads_take) {
        return 0;
    }
    int amount_taken = building_granary_try_remove_resource(granary, resource, granary_loads_take);

    // create delivery boys
    int type = FIGURE_DELIVERY_BOY;
    if (f->type == FIGURE_MESS_HALL_SUPPLIER) {
        type = FIGURE_MESS_HALL_COLLECTOR;
    } else if (f->type == FIGURE_CARAVANSERAI_SUPPLIER) {
        type = FIGURE_CARAVANSERAI_COLLECTOR;
    }
    int leader_id = f->id;
    int previous_boy = f->id;
    for (int i = 0; i < amount_taken; i++) {
        previous_boy = figure_supplier_create_delivery_boy(previous_boy, leader_id, type);
    }
    return 1;
}

// Venus Grand Temple wine
static int take_resource_from_generic_building(figure *f, int building_id)
{
    building *b = building_get(building_id);
    int num_loads;
    int stored = b->resources[RESOURCE_WINE];
    if (stored < 2) {
        num_loads = stored;
    } else {
        num_loads = 2;
    }
    if (num_loads <= 0) {
        return 0;
    }
    b->resources[RESOURCE_WINE] -= num_loads;

    // create delivery boys
    int priest_id = f->id;
    int boy1 = figure_supplier_create_delivery_boy(priest_id, priest_id, FIGURE_DELIVERY_BOY);
    if (num_loads > 1) {
        figure_supplier_create_delivery_boy(boy1, priest_id, FIGURE_DELIVERY_BOY);
    }
    return 1;
}

static int take_resource_from_warehouse(figure *f, int warehouse_id, int max_amount)
{
    building *warehouse = building_get(warehouse_id);
    if (warehouse->type != BUILDING_WAREHOUSE) {
        return take_resource_from_generic_building(f, warehouse_id);
    }
    int num_loads;
    int stored = building_warehouse_get_available_amount(warehouse, f->collecting_item_id);
    if (stored < max_amount) {
        num_loads = stored;
    } else {
        num_loads = max_amount;
    }
    if (num_loads <= 0) {
        return 0;
    }
    int amount_taken = building_warehouse_try_remove_resource(warehouse, f->collecting_item_id, num_loads);
    if (amount_taken <= 0) {
        return 0;
    }

    // Track how many loads the supplier is carrying so the return code knows
    // how much to deposit. Lighthouse and Highway Station don't spawn delivery boys.
    if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER || f->type == FIGURE_HIGHWAY_STATION_SUPPLIER) {
        f->loads_sold_or_carrying = amount_taken;
    } else {
        // create delivery boys (one per load above the first)
        int supplier_id = f->id;
        int boy1 = figure_supplier_create_delivery_boy(supplier_id, supplier_id, FIGURE_DELIVERY_BOY);
        if (amount_taken > 1) {
            figure_supplier_create_delivery_boy(boy1, supplier_id, FIGURE_DELIVERY_BOY);
        }
    }
    return 1;
}

static int destination_can_supply(building *dest, resource_type resource, building_type consumer_type)
{
    if (!dest || !building_is_active(dest) || dest->has_plague) {
        return 0;
    }
    if (dest->type == BUILDING_GRANARY || dest->type == BUILDING_WAREHOUSE) {
        building_storage_permission_states permission =
            building_storage_get_permission_from_building_type(consumer_type);
        if (!building_storage_get_permission(permission, dest)) {
            return 0;
        }
    }
    if (dest->type == BUILDING_GRANARY && resource_is_food(resource)) {
        return building_granary_count_available_resource(dest, resource, 1) > 0;
    }
    if (dest->type == BUILDING_WAREHOUSE) {
        if (city_resource_is_stockpiled(resource)) {
            return 0;
        }
        return building_warehouse_get_available_amount(dest, resource) > 0;
    }
    /* Generic buildings (e.g. Venus grand temple wine store) */
    return dest->resources[resource] > 0;
}

static int home_wants_more_of_resource(figure *f)
{
    building *home = building_get(f->building_id);
    resource_type resource = f->collecting_item_id;
    if (!resource || resource == RESOURCE_NONE) {
        return 0;
    }
    int stock = home->resources[resource];
    if (!resource_is_food(resource)) {
        if (home->type == BUILDING_LIGHTHOUSE) {
            return stock < 500; /* matches lighthouse MAX_TIMBER */
        }
        if (home->type == BUILDING_HIGHWAY_STATION) {
            int max_stock = building_highway_station_max_stock();
            return max_stock > 0 && stock < max_stock;
        }
        return 1;
    }
    if (home->type == BUILDING_MESS_HALL) {
        return stock < figure_supplier_max_stocked_mess_hall_adjusted();
    }
    if (home->type == BUILDING_CARAVANSERAI) {
        return stock < MAX_FOOD_STOCKED_CARAVANSERAI;
    }
    return stock < MAX_FOOD_STOCKED_MARKET;
}

static int change_market_supplier_destination(figure *f, int dst_building_id)
{
    if (!dst_building_id) {
        return 0;
    }
    building *b_dst = building_get(dst_building_id);
    map_point road = { 0 };
    int has_road_access = 0;
    if (b_dst->type == BUILDING_WAREHOUSE) {
        has_road_access = map_has_road_access_warehouse(b_dst->x, b_dst->y, &road);
    } else if (b_dst->type == BUILDING_GRANARY) {
        has_road_access = map_has_road_access_granary(b_dst->x, b_dst->y, &road);
    } else if (map_has_road_access(b_dst->x, b_dst->y, b_dst->size, &road)) {
        /* Venus grand temple / other generic pickups */
        has_road_access = 1;
    }
    if (!has_road_access) {
        return 0;
    }

    if (f->destination_building_id && f->destination_building_id != dst_building_id) {
        f->last_destinatation_id = f->destination_building_id;
    }
    figure_route_remove(f);
    f->destination_building_id = dst_building_id;
    f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
    f->destination_x = road.x;
    f->destination_y = road.y;
    f->wait_ticks = 0;
    return 1;
}

static int is_better_destination(figure *f, resource_type r, resource_storage_info *info)
{
    if (!f->destination_building_id) {
        return 1;
    }
    building *home = building_get(f->building_id);
    building *old_dest = building_get(f->destination_building_id);
    /* Current target unusable for pickup — always switch (cartpusher pattern) */
    if (!destination_can_supply(old_dest, r, home->type)) {
        return 1;
    }
    /* Prefer not to return to a dest that just rejected us */
    if (f->last_destinatation_id && f->destination_building_id == f->last_destinatation_id &&
        info->building_id && info->building_id != f->destination_building_id) {
        return 1;
    }
    /* New building half the distance or closer — avoid ping-pong */
    int old_dest_dist = building_dist(f->x, f->y, 1, 1, old_dest);
    if (info->min_distance <= old_dest_dist / 2) {
        return 1;
    }
    return 0;
}

static int supplier_max_search_distance(building *home)
{
    if (home->type == BUILDING_MARKET) {
        return config_get(CONFIG_GP_CH_MARKET_RANGE) ? MARKET_MAX_DISTANCE : map_data.width;
    }
    if (home->type == BUILDING_MESS_HALL || home->type == BUILDING_HIGHWAY_STATION) {
        return 40;
    }
    return map_data.width;
}

static int supplier_lookup_storage_from_home(building *home)
{
    switch (home->type) {
        case BUILDING_MARKET:
            return building_market_get_storage_destination(home);
        case BUILDING_MESS_HALL:
            return building_mess_hall_get_storage_destination(home);
        case BUILDING_CARAVANSERAI:
            return building_caravanserai_get_storage_destination(home);
        case BUILDING_LIGHTHOUSE:
            return building_lighthouse_get_storage_destination(home);
        case BUILDING_HIGHWAY_STATION:
            return building_highway_station_get_storage_destination(home);
        case BUILDING_TAVERN:
            return building_tavern_get_storage_destination(home);
        default:
            if (building_is_ceres_temple(home->type) || building_is_venus_temple(home->type)) {
                return building_temple_get_storage_destination(home);
            }
            return 0;
    }
}

static int recalculate_supplier_destination(figure *f)
{
    int item = f->collecting_item_id;
    building *home = building_get(f->building_id);
    resource_storage_info info[RESOURCE_MAX] = { 0 };

    /* Stay on source road network so mid-trip recalcs don't stall on a foreign net */
    int road_network = home->road_network_id;
    int max_distance = supplier_max_search_distance(home);

    int has_needed;
    if (home->type == BUILDING_MARKET) {
        has_needed = building_market_get_needed_inventory(home, info);
    } else if (home->type == BUILDING_LIGHTHOUSE) {
        info[RESOURCE_TIMBER].needed = 1;
        has_needed = 1;
    } else if (home->type == BUILDING_HIGHWAY_STATION) {
        info[RESOURCE_STONE].needed = 1;
        info[RESOURCE_SAND].needed = 1;
        has_needed = 1;
    } else {
        has_needed = building_distribution_get_handled_resources_for_building(home, info);
    }

    if (!has_needed ||
        !building_distribution_get_resource_storages_for_figure(info, home->type, road_network, f, max_distance)) {
        return 0;
    }

    /* Same dest is fine only if it can still supply the current item */
    if (item && info[item].building_id &&
        (f->destination_building_id == info[item].building_id || f->building_id == info[item].building_id)) {
        if (destination_can_supply(building_get(info[item].building_id), item, home->type)) {
            return 1;
        }
        /* Fall through — try another building / resource */
    }

    if (item && info[item].building_id) {
        if (is_better_destination(f, item, &info[item])) {
            /* Keep collecting_item_id; only change storage */
            home->data.market.fetch_inventory_id = item;
            return change_market_supplier_destination(f, info[item].building_id);
        }
        /* Keep current dest if it is still usable */
        if (destination_can_supply(building_get(f->destination_building_id), item, home->type)) {
            return 1;
        }
    }

    resource_type fetch_inventory = RESOURCE_NONE;
    if (home->type == BUILDING_MARKET) {
        fetch_inventory = building_market_fetch_inventory(home, info);
    } else if (home->type == BUILDING_LIGHTHOUSE) {
        fetch_inventory = info[RESOURCE_TIMBER].building_id ? RESOURCE_TIMBER : RESOURCE_NONE;
    } else if (home->type == BUILDING_HIGHWAY_STATION) {
        fetch_inventory = building_distribution_fetch(home, info, 0, 1);
        if (fetch_inventory == RESOURCE_NONE) {
            fetch_inventory = building_distribution_fetch(home, info, building_highway_station_max_stock(), 0);
        }
    } else {
        fetch_inventory = building_distribution_fetch(home, info, 0, 1);
        if (fetch_inventory == RESOURCE_NONE) {
            fetch_inventory = building_distribution_fetch(home, info, BASELINE_STOCK, 0);
        }
    }
    if (fetch_inventory == RESOURCE_NONE || !info[fetch_inventory].building_id) {
        return 0;
    }
    /* Don't bounce back to a storage that just rejected us unless nothing else exists */
    if (info[fetch_inventory].building_id == f->last_destinatation_id &&
        !destination_can_supply(building_get(f->last_destinatation_id), fetch_inventory, home->type)) {
        return 0;
    }
    home->data.market.fetch_inventory_id = fetch_inventory;
    f->collecting_item_id = fetch_inventory;
    return change_market_supplier_destination(f, info[fetch_inventory].building_id);
}

/* After a failed take (empty / maintaining / permission): find another storage or give up. */
static int try_reroute_supplier_after_rejection(figure *f)
{
    if (!home_wants_more_of_resource(f)) {
        return 0;
    }
    f->last_destinatation_id = f->destination_building_id;
    f->destination_building_id = 0;
    f->destination_x = 0;
    f->destination_y = 0;
    figure_route_remove(f);
    f->wait_ticks = 0;

    if (recalculate_supplier_destination(f)) {
        return 1;
    }
    /* Fall back to home-centric lookup used at spawn (covers temple wine, etc.) */
    building *home = building_get(f->building_id);
    int dst = supplier_lookup_storage_from_home(home);
    if (dst && dst != f->last_destinatation_id) {
        f->collecting_item_id = home->data.market.fetch_inventory_id;
        return change_market_supplier_destination(f, dst);
    }
    return 0;
}

static void supplier_return_home_empty(figure *f)
{
    f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
    f->collecting_item_id = RESOURCE_NONE;
    f->loads_sold_or_carrying = 0;
    f->destination_x = f->source_x;
    f->destination_y = f->source_y;
    f->destination_building_id = f->building_id;
    f->wait_ticks = 0;
    figure_route_remove(f);
}

void figure_supplier_action(figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    f->use_cross_country = 0;
    f->max_roam_length = 800;

    building *b = building_get(f->building_id);
    if (b->state != BUILDING_STATE_IN_USE ||
        (b->figure_id2 != f->id && b->figure_id != f->id && b->figure_id4 != f->id)) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->wait_ticks = 0;
                f->previous_tile_x = f->x;
                f->previous_tile_y = f->y;
                int id = f->id;
                int took = 0;
                if (!resource_is_food(f->collecting_item_id)) {
                    int max_amount;
                    if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER) {
                        max_amount = 1;
                    } else if (f->type == FIGURE_HIGHWAY_STATION_SUPPLIER) {
                        max_amount = 4; // larger trips so monthly consumption can keep accumulating
                    } else {
                        max_amount = 2;
                    }
                    took = take_resource_from_warehouse(f, f->destination_building_id, max_amount);
                } else {
                    took = take_food_from_granary(f, f->building_id, f->destination_building_id);
                }
                f = figure_get(id);
                if (took) {
                    f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                } else if (try_reroute_supplier_after_rejection(f)) {
                    /* New storage assigned — stay in GOING_TO_STORAGE (cartpusher pattern) */
                } else {
                    supplier_return_home_empty(f);
                }
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                /* Repath only — do not abort the trip */
                figure_route_remove(f);
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_LOST) {
                if (!try_reroute_supplier_after_rejection(f)) {
                    supplier_return_home_empty(f);
                }
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->wait_ticks = 0;
                /* Drop dead/rejected targets; best-effort improve path if still valid */
                building *home = building_get(f->building_id);
                building *dest = building_get(f->destination_building_id);
                if (!destination_can_supply(dest, f->collecting_item_id, home->type)) {
                    if (!try_reroute_supplier_after_rejection(f)) {
                        supplier_return_home_empty(f);
                    }
                } else {
                    recalculate_supplier_destination(f);
                }
            }
            break;
        case FIGURE_ACTION_146_SUPPLIER_RETURNING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                /* Only deposit if we actually took something (empty re-route returns carry nothing) */
                if (f->direction == DIR_FIGURE_AT_DESTINATION && f->loads_sold_or_carrying > 0) {
                    if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER) {
                        building_get(f->building_id)->resources[RESOURCE_TIMBER] +=
                            100 * f->loads_sold_or_carrying;
                    } else if (f->type == FIGURE_HIGHWAY_STATION_SUPPLIER &&
                        (f->collecting_item_id == RESOURCE_STONE || f->collecting_item_id == RESOURCE_SAND)) {
                        building *target = building_get(f->building_id);
                        target->resources[f->collecting_item_id] += f->loads_sold_or_carrying * 100;
                        building_highway_station_refresh_graphic(target);
                    }
                }
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            }
            break;
    }
    if (f->type == FIGURE_MESS_HALL_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                if (f->attack_image_offset < 14) {
                    f->image_id = assets_get_image_id("Walkers", "quartermaster_f_ne_01") + dir * 6;
                } else {
                    f->image_id = assets_get_image_id("Walkers", "quartermaster_f_ne_01") + dir * 6 + ((f->attack_image_offset - 14) / 2);
                }
                break;
            case FIGURE_ACTION_149_CORPSE:
                f->image_id = assets_get_image_id("Walkers", "quartermaster_death_01") +
                    figure_image_corpse_offset(f);
                break;
            default:
                f->image_id = assets_get_image_id("Walkers", "quartermaster_ne_01") +
                    dir * 12 + f->image_offset;
                break;
        }
    } else if (f->type == FIGURE_PRIEST_SUPPLIER) {
        figure_image_update(f, image_group(GROUP_FIGURE_PRIEST));
    } else if (f->type == FIGURE_BARKEEP_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "Barkeep Death 01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "Barkeep NE 01") +
                dir * 12 + f->image_offset;
        }
    } else if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER || f->type == FIGURE_HIGHWAY_STATION_SUPPLIER) {
        if (f->action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING) {
            f->cart_image_id = resource_get_data(f->collecting_item_id)->image.cart.single_load;
        } else {
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART);
        }
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = image_group(GROUP_FIGURE_CARTPUSHER) + figure_image_corpse_offset(f) + 96;
            f->cart_image_id = 0;
        } else {
            f->image_id = image_group(GROUP_FIGURE_CARTPUSHER) + dir + 8 * f->image_offset;
        }
        if (f->cart_image_id) {
            f->cart_image_id += dir;
            figure_image_set_cart_offset(f, dir);
        }
    } else if (f->type == FIGURE_CARAVANSERAI_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_overseer_death_01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_overseer_ne_01") +
                dir * 12 + f->image_offset;
        }
    } else {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "marketbuyer_death_01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "marketbuyer_ne_01") +
                dir * 12 + f->image_offset;
        }
    }
}

void figure_delivery_boy_action(figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;

    figure *leader = figure_get(f->leading_figure_id);
    if (f->leading_figure_id <= 0 || leader->action_state == FIGURE_ACTION_149_CORPSE) {
        f->state = FIGURE_STATE_DEAD;
    } else {
        if (leader->state == FIGURE_STATE_ALIVE) {
            if (leader->type == FIGURE_MARKET_SUPPLIER || leader->type == FIGURE_DELIVERY_BOY ||
                leader->type == FIGURE_MESS_HALL_SUPPLIER || leader->type == FIGURE_MESS_HALL_COLLECTOR ||
                leader->type == FIGURE_PRIEST_SUPPLIER || leader->type == FIGURE_PRIEST ||
                leader->type == FIGURE_BARKEEP_SUPPLIER || leader->type == FIGURE_CARAVANSERAI_SUPPLIER ||
                leader->type == FIGURE_CARAVANSERAI_COLLECTOR) {
                figure_movement_follow_ticks(f, 1);
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
        } else { // leader arrived at market, drop resource at market
            building_get(f->building_id)->resources[f->collecting_item_id] += 100;
            f->state = FIGURE_STATE_DEAD;
        }
    }
    if (leader->is_ghost && !leader->height_adjusted_ticks) {
        f->is_ghost = 1;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->type == FIGURE_MESS_HALL_COLLECTOR) {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "M Hall death 01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "M Hall NE 01") +
                dir * 12 + f->image_offset;
        }
    } else if (f->type == FIGURE_CARAVANSERAI_COLLECTOR) {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_walker_death_01") + figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_walker_ne_01")
                + dir * 12 + f->image_offset;
        }
    } else {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = image_group(GROUP_FIGURE_DELIVERY_BOY) + 96 +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = image_group(GROUP_FIGURE_DELIVERY_BOY) +
                dir + 8 * f->image_offset;
        }
    }
}

void figure_fort_supplier_action(figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);

    building *b = building_get(f->building_id);
    if (!b || b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_MESS_HALL) {
        f->state = FIGURE_STATE_DEAD;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_236_SUPPLY_POST_GOING_TO_FORT:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_237_SUPPLY_POST_RETURNING_FROM_FORT;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                f->wait_ticks = 20;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_237_SUPPLY_POST_RETURNING_FROM_FORT:
            if (f->wait_ticks) {
                f->wait_ticks--;
            } else {
                figure_movement_move_ticks(f, 1);
                if (f->direction == DIR_FIGURE_REROUTE) {
                    figure_route_remove(f);
                } else if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
    }

    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->image_id = assets_get_image_id("Walkers", "M Hall death 01") +
            figure_image_corpse_offset(f);
    } else {
        f->image_id = assets_get_image_id("Walkers", "M Hall NE 01") +
            dir * 12 + f->image_offset;
    }
}
