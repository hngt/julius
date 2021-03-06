#ifndef BUILDING_INDUSTRY_H
#define BUILDING_INDUSTRY_H

#include "building/building.h"

int building_is_farm(building_type type);
int building_is_workshop(building_type type);

void building_industry_update_production();
void building_industry_update_wheat_production();

int building_industry_has_produced_resource(building *b);
void building_industry_start_new_production(building *b);

void building_bless_farms();
void building_curse_farms(int big_curse);

void building_workshop_add_raw_material(building *b);

int building_get_workshop_for_raw_material(int x, int y, int resource, int distance_from_entry, int road_network_id, int *x_dst, int *y_dst);
int building_get_workshop_for_raw_material_with_room(int x, int y, int resource, int distance_from_entry, int road_network_id, int *x_dst, int *y_dst);

#endif // BUILDING_INDUSTRY_H
