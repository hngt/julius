#include "empire.h"

#include "building/count.h"
#include "city/constants.h"
#include "city/population.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/io.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/trade_route.h"

enum {
    EMPIRE_WIDTH = 2000,
    EMPIRE_HEIGHT = 1000
};

static struct {
    int initial_scroll_x;
    int initial_scroll_y;
    int scroll_x;
    int scroll_y;
    int selected_object;
    int viewport_width;
    int viewport_height;
} data;

void empire_load(int is_custom_scenario, int empire_id)
{
    char raw_data[12800];
    const char *filename = is_custom_scenario ? "c32.emp" : "c3.emp";
    
    // read header with scroll positions
    io_read_file_part_into_buffer(filename, raw_data, 4, 32 * empire_id);
    buffer buf;
    buffer_init(&buf, raw_data, 4);
    data.initial_scroll_x = buffer_read_i16(&buf);
    data.initial_scroll_y = buffer_read_i16(&buf);

    // read data section with objects
    int offset = 1280 + 12800 * empire_id;
    io_read_file_part_into_buffer(filename, raw_data, 12800, offset);
    buffer_init(&buf, raw_data, 12800);
    empire_object_load(&buf);
}

void empire_init_scenario()
{
    data.scroll_x = data.initial_scroll_x;
    data.scroll_y = data.initial_scroll_y;
    data.viewport_width = EMPIRE_WIDTH;
    data.viewport_height = EMPIRE_HEIGHT;

    empire_object_init_cities();
}

static void check_scroll_boundaries()
{
    int max_x = EMPIRE_WIDTH - data.viewport_width;
    int max_y = EMPIRE_HEIGHT - data.viewport_height;

    data.scroll_x = calc_bound(data.scroll_x, 0, max_x);
    data.scroll_y = calc_bound(data.scroll_y, 0, max_y);
}

void empire_set_viewport(int width, int height)
{
    data.viewport_width = width;
    data.viewport_height = height;
    check_scroll_boundaries();
}

void empire_adjust_scroll(int *x_offset, int *y_offset)
{
    *x_offset = *x_offset - data.scroll_x;
    *y_offset = *y_offset - data.scroll_y;
}

void empire_scroll_map(int direction)
{
    if (direction == DIR_8_NONE) {
        return;
    }
    switch (direction) {
        case DIR_0_TOP:
            data.scroll_y -= 20;
            break;
        case DIR_1_TOP_RIGHT:
            data.scroll_x += 20;
            data.scroll_y -= 20;
            break;
        case DIR_2_RIGHT:
            data.scroll_x += 20;
            break;
        case DIR_3_BOTTOM_RIGHT:
            data.scroll_x += 20;
            data.scroll_y += 20;
            break;
        case DIR_4_BOTTOM:
            data.scroll_y += 20;
            break;
        case DIR_5_BOTTOM_LEFT:
            data.scroll_x -= 20;
            data.scroll_y += 20;
            break;
        case DIR_6_LEFT:
            data.scroll_x -= 20;
            break;
        case DIR_7_TOP_LEFT:
            data.scroll_x -= 20;
            data.scroll_y -= 20;
            break;
    }
    check_scroll_boundaries();
}

int empire_selected_object()
{
    return data.selected_object;
}

void empire_clear_selected_object()
{
    data.selected_object = 0;
}

void empire_select_object(int x, int y)
{
    int map_x = x + data.scroll_x;
    int map_y = y + data.scroll_y;
    
    data.selected_object = empire_object_get_closest(map_x, map_y);
}

int empire_can_export_resource_to_city(int city_id, int resource)
{
    empire_city *city = empire_city_get(city_id);
    if (city_id && trade_route_limit_reached(city->route_id, resource)) {
        // quota reached
        return 0;
    }
    if (city_resource_count(resource) <= city_resource_export_over(resource)) {
        // stocks too low
        return 0;
    }
    if (city_id == 0 || city->buys_resource[resource]) {
        return city_resource_trade_status(resource) == TRADE_STATUS_EXPORT;
    } else {
        return 0;
    }
}

static int get_max_stock_for_population()
{
    int population = city_population();
    if (population < 2000) {
        return 10;
    } else if (population < 4000) {
        return 20;
    } else if (population < 6000) {
        return 30;
    } else {
        return 40;
    }
}

int empire_can_import_resource_from_city(int city_id, int resource)
{
    empire_city *city = empire_city_get(city_id);
    if (!city->sells_resource[resource]) {
        return 0;
    }
    if (city_resource_trade_status(resource) != TRADE_STATUS_IMPORT) {
        return 0;
    }
    if (trade_route_limit_reached(city->route_id, resource)) {
        return 0;
    }

    int in_stock = city_resource_count(resource);
    int max_in_stock = 0;
    int finished_good = RESOURCE_NONE;
    switch (resource) {
        // food and finished materials
        case RESOURCE_WHEAT:
        case RESOURCE_VEGETABLES:
        case RESOURCE_FRUIT:
        case RESOURCE_MEAT:
        case RESOURCE_POTTERY:
        case RESOURCE_FURNITURE:
        case RESOURCE_OIL:
        case RESOURCE_WINE:
            max_in_stock = get_max_stock_for_population();
            break;

        case RESOURCE_MARBLE:
        case RESOURCE_WEAPONS:
            max_in_stock = 10;
            break;

        case RESOURCE_CLAY:
            finished_good = RESOURCE_POTTERY;
            break;
        case RESOURCE_TIMBER:
            finished_good = RESOURCE_FURNITURE;
            break;
        case RESOURCE_OLIVES:
            finished_good = RESOURCE_OIL;
            break;
        case RESOURCE_VINES:
            finished_good = RESOURCE_WINE;
            break;
        case RESOURCE_IRON:
            finished_good = RESOURCE_WEAPONS;
            break;
    }
    if (finished_good) {
        max_in_stock = 2 + 2 * building_count_industry_active(finished_good);
    }
    return in_stock < max_in_stock ? 1 : 0;
}

void empire_save_state(buffer *buf)
{
    buffer_write_i32(buf, data.scroll_x);
    buffer_write_i32(buf, data.scroll_y);
    buffer_write_i32(buf, data.selected_object);
}

void empire_load_state(buffer *buf)
{
    data.scroll_x = buffer_read_i32(buf);
    data.scroll_y = buffer_read_i32(buf);
    data.selected_object = buffer_read_i32(buf);
}
