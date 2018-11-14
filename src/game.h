#ifndef __GAME_H__
#define __GAME_H__

#include "main.h"

#define mp(x, y) std::make_pair(x, y)
#define min(x, y) std::min(x, y)
#define max(x, y) std::max(x, y)

const int offset[][2] = {
    {1,-1},{2,0},{1,1},{0,2},{-1,1},{-2,0},{-1,-1},{0,-2}
};

const int offset_tbl[][2] = {
    {1,-1},{2,0},{1,1},{0,2},{-1,1},{-2,0},{-1,-1},{0,-2},
    {2,-2},{2,2},{-2,2},{-2,-2},{1,-3},{3,-1},{3,1},{1,3},
    {-1,3},{-3,1},{-3,-1},{-1,-3},{4,0},{-4,0},{0,4},{0,-4},
    {1,-5},{2,-4},{3,-3},{4,-2},{5,-1},{5,1},{4,2},{3,3},
    {2,4},{1,5},{-1,5},{-2,4},{-3,3},{-4,2},{-5,1},{-5,-1},
    {-4,-2},{-3,-3},{-2,-4},{-1,-5},{0,6},{6,0},{0,-6},{-6,0},
    {0,-8},{1,-7},{2,-6},{3,-5},{4,-4},{5,-3},{6,-2},{7,-1},
    {8,0},{7,1},{6,2},{5,3},{4,4},{3,5},{2,6},{1,7},
    {0,8},{-1,7},{-2,6},{-3,5},{-4,4},{-5,3},{-6,2},{-7,1},
    {-8,0},{-7,-1},{-6,-2},{-5,-3},{-4,-4},{-3,-5},{-2,-6},{-1,-7},
};

char* prod_name(int prod);
int mineral_cost(int fac, int prod);
bool knows_tech(int fac, int tech);
bool has_ability(int fac, int abl);
bool has_chassis(int fac, int chs);
bool has_weapon(int fac, int wpn);
bool has_terra(int fac, int act);
bool has_project(int fac, int id);
bool has_facility(int base_id, int id);
bool can_build(int base_id, int id);
bool at_war(int a, int b);
int find_hq(int fac);
int veh_triad(int id);
int unit_triad(int id);
int unit_speed(int id);
int best_armor(int fac, bool cheap);
int best_weapon(int fac);
int best_reactor(int fac);
int offense_value(UNIT* u);
int defense_value(UNIT* u);
int random(int n);
int wrap(int a);
int map_range(int x1, int y1, int x2, int y2);
MAP* mapsq(int x, int y);
int unit_in_tile(MAP* sq);
int set_move_to(int id, int x, int y);
int set_road_to(int id, int x, int y);
int set_action(int id, int act, char icon);
int set_convoy(int id, int res);
int at_target(VEH* veh);
bool is_ocean(MAP* sq);
bool is_ocean_shelf(MAP* sq);
bool water_base(int id);
bool workable_tile(int x, int y, int fac);
int nearby_items(int x, int y, int range, uint32_t item);
int bases_in_range(int x, int y, int range);
int nearby_tiles(int x, int y, int type, int limit);
int coast_tiles(int x, int y);
void print_map(int x, int y);
void print_veh(int id);

class TileSearch {
    int type;
    int head;
    int tail;
    int items;
    int y_skip;
    MAP* sq;
    std::pair<int, int> newtiles[QSIZE];
    public:
    int rx, ry;
    std::set<std::pair <int, int>> oldtiles;
    void init(int, int, int);
    void init(int, int, int, int);
    int visited();
    MAP* get_next();
};


#endif // __GAME_H__
