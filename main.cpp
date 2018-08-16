using namespace std;

#include "main.h"
#include "game.h"

const char* VERSION = "ThinkerV0.5";
const char* VERSION_DATE = __DATE__;

FILE* debug_log;
Config conf;

int base_mins[BASES];

static int handler(void* user, const char* section, const char* name, const char* value) {
    Config* pconfig = (Config*)user;
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("thinker", "free_formers")) {
        pconfig->free_formers = atoi(value);
    } else if (MATCH("thinker", "satellites_nutrient")) {
        pconfig->satellites_nutrient = max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_mineral")) {
        pconfig->satellites_mineral = max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_energy")) {
        pconfig->satellites_energy = max(0, atoi(value));
    } else if (MATCH("thinker", "factions_enabled")) {
        pconfig->factions_enabled = atoi(value);
    } else if (MATCH("thinker", "terraform_ai")) {
        pconfig->terraform_ai = atoi(value);
    } else if (MATCH("thinker", "production_ai")) {
        pconfig->production_ai = atoi(value);
    } else if (MATCH("thinker", "tech_balance")) {
        pconfig->tech_balance = atoi(value);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE UNUSED(hinstDLL), DWORD fdwReason, LPVOID UNUSED(lpvReserved)) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            memset(base_mins, 0, BASES);
            conf.free_formers = 0;
            conf.satellites_nutrient = 0;
            conf.satellites_mineral = 0;
            conf.satellites_energy = 0;
            conf.factions_enabled = 7;
            conf.terraform_ai = 1;
            conf.production_ai = 1;
            conf.tech_balance = 1;
            debug_log = fopen("debug.txt", "w");
            if (!debug_log)
                return FALSE;
            if (ini_parse("thinker.ini", handler, &conf) < 0)
                return FALSE;
            *tx_version = VERSION;
            *tx_date = VERSION_DATE;
            break;

        case DLL_PROCESS_DETACH:
            fclose(debug_log);
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

DLL_EXPORT int ThinkerDecide(int mode, int id, int val1, int val2) {
    assert(mode > 0 && val1 >= 0 && val2 > -128 && id >= 0 && id <= 2048);

    if (mode == 2) {
        return terraform_action(id, val1-4, val2);
    } else if (mode == 3) {
        return tech_value(id, val1, val2);
    } else if (mode == 4) {
        VEH* veh = &tx_vehicles[id];
        if (conf.terraform_ai && veh->faction_id <= conf.factions_enabled) {
            int wmode = tx_units[veh->proto_id].weapon_mode;
            if (wmode == WMODE_COLONIST)
                return consider_base(id);
            else if (wmode == WMODE_CONVOY)
                return consider_convoy(id);
        }
        return tx_enemy_move(id);
    } else if (mode == 5) {
        return turn_upkeep();
    } else if (mode != 1) {
        return 0;
    }

    BASE base = tx_bases[id];
    int owner = base.faction_id;
    int last_choice = 0;

    if (DEBUG) {
        fprintf(debug_log, "[ turn: %d faction: %d base: %2d x: %2d y: %2d "\
        "pop: %d tal: %d dro: %d mins: %2d acc: %2d prod: %3d | %s | %s ]\n",
        *tx_current_turn, owner, id, base.x_coord, base.y_coord,
        base.pop_size, base.talent_total, base.drone_total,
        base.mineral_surplus, base.minerals_accumulated,
        base.queue_production_id[0], prod_name(base.queue_production_id[0]),
        (char*)&(base.name));
    }

    if (1 << owner & *tx_human_players) {
        if (DEBUG) fprintf(debug_log, "skipping human base\n");
        last_choice = base.queue_production_id[0];
    } else if (!conf.production_ai || owner > conf.factions_enabled) {
        if (DEBUG) fprintf(debug_log, "skipping computer base\n");
        last_choice = tx_base_prod_choices(id, 0, 0, 0);
    } else {
        last_choice = select_prod(id);
        if (DEBUG) fprintf(debug_log, "choice: %d %s\n", last_choice, prod_name(last_choice));
    }
    base_mins[id] = tx_bases[id].minerals_accumulated;
    fflush(debug_log);
    return last_choice;
}

int turn_upkeep() {
    for (int i=1; i<8; i++) {
        if (1 << i & *tx_human_players)
            continue;
        if (knows_tech(i, TECH_PlaNets)) {
            if (knows_tech(i, TECH_DocFlex))
                tx_propose_proto(i, CHSI_FOIL, WPN_PROBE_TEAM, ARM_NO_ARMOR, 0,
                    RECT_FISSION, PLAN_INFO_WARFARE, "Foil Probe Team");
            if (knows_tech(i, TECH_NanoMin))
                tx_propose_proto(i, CHSI_HOVERTANK, WPN_PROBE_TEAM, ARM_NO_ARMOR, ABL_ALGO_ENHANCEMENT,
                    RECT_FUSION, PLAN_INFO_WARFARE, "Enhanced Probe Team");
        }
    }
    if (*tx_current_turn == 1) {
        int bonus = ~(*tx_human_players) & 0xfe;
        for (int i=0; i<*tx_total_num_vehicles; i++) {
            VEH v = tx_vehicles[i];
            if (1 << v.faction_id & bonus) {
                bonus &= ~(1 << v.faction_id);
                for (int j=0; j<conf.free_formers; j++) {
                    int veh = tx_veh_init(BSC_FORMERS, v.faction_id, v.x_coord, v.y_coord);
                    tx_vehicles[veh].home_base_id = -1;
                }
                tx_factions[v.faction_id].satellites_nutrient = conf.satellites_nutrient;
                tx_factions[v.faction_id].satellites_mineral = conf.satellites_mineral;
                tx_factions[v.faction_id].satellites_energy = conf.satellites_energy;
            }
        }
    }
    if (DEBUG) {
        int* tx_game_rules = (int*)0x9A64C0;
        *tx_game_rules |= RULES_DEBUG_MODE;
    }

    return 0;
}

int tech_value(int tech, int fac, int value) {
    if (conf.tech_balance && fac <= conf.factions_enabled) {
        if (tech == TECH_Ecology || tech == tx_basic->tech_preq_allow_3_energy_sq
        || tech == tx_basic->tech_preq_allow_3_minerals_sq
        || tech == tx_basic->tech_preq_allow_3_nutrients_sq) {
            value *= 2;
        }
    }
    debuglog("tech_value %d %d %d %s\n", tech, fac, value, tx_techs[tech].name);
    return value;
}

void print_veh(int id) {
    VEH v = tx_vehicles[id];
    fprintf(debug_log, "VEH %16s %2d | %08x %04x | %2d %3d | %2d %2d %2d %2d | %d %d %d %d %d\n",
        (char*)&(tx_units[v.proto_id].name), id,
        v.flags_1, v.flags_2, v.move_status, v.target_action,
        v.x_coord, v.y_coord, v.waypoint_1_x_coord, v.waypoint_1_y_coord,
        v.unk4, v.unk5, v.unk6, v.unk8, v.unk9);
}

bool has_project(int fac, int id) {
    int i = tx_secret_projects[id-70];
    if (i >= 0 && tx_bases[i].faction_id == fac)
        return true;
    return false;
}

bool has_facility(int base_id, int id) {
    if (id >= 70)
        return tx_secret_projects[id-70] != -1;
    int fac = tx_bases[base_id].faction_id;
    const int freebies[] = {
        FAC_COMMAND_CENTER, FAC_COMMAND_NEXUS,
        FAC_NAVAL_YARD, FAC_MARITIME_CONTROL_CENTER,
        FAC_ENERGY_BANK, FAC_PLANETARY_ENERGY_GRID,
        FAC_PERIMETER_DEFENSE, FAC_CITIZENS_DEFENSE_FORCE,
        FAC_AEROSPACE_COMPLEX, FAC_CLOUDBASE_ACADEMY,
        FAC_BIOENHANCEMENT_CENTER, FAC_CYBORG_FACTORY};
    for (int i=0; i<12; i+=2) {
        if (freebies[i] == id && has_project(fac, freebies[i+1]))
            return true;
    }
    BASE* base = &tx_bases[base_id];
    int val = base->facilities_built[ id/8 ] & (1 << (id % 8));
    return val != 0;
}

int find_hq(int faction) {
    for(int i=0; i<*tx_total_num_bases; i++) {
        BASE* base = &tx_bases[i];
        if (base->faction_id == faction && has_facility(i, FAC_HEADQUARTERS)) {
            return i;
        }
    }
    return -1;
}

bool can_build(int base_id, int id) {
    BASE* base = &tx_bases[base_id];
    if (id == FAC_STOCKPILE_ENERGY)
        return false;
    if (id == FAC_HEADQUARTERS && find_hq(base->faction_id) >= 0)
        return false;
    if (id == FAC_RECYCLING_TANKS && has_facility(base_id, FAC_PRESSURE_DOME))
        return false;
    return knows_tech(base->faction_id, tx_facilities[id].preq)
    && !has_facility(base_id, id);
}

bool project_capacity(int fac) {
    int bases = tx_factions[fac].current_num_bases;
    int n = 0;
    for (int i=0; i<*tx_total_num_bases; i++) {
        if (tx_bases[i].faction_id == fac && tx_bases[i].queue_production_id[0] <= -70)
            n++;
    }
    return (n < 3 && n < bases/4);
}

int find_project(int faction, int max_cost) {
    int projs[40];
    int n = 0;
    for (int i=70; i<107; i++) {
        if (tx_secret_projects[i-70] == -1 && knows_tech(faction, tx_facilities[i].preq)
        && tx_facilities[i].cost <= max_cost) {
            if (DEBUG) fprintf(debug_log,
                "find_project %d %d %s\n", faction, i, (char*)tx_facilities[i].name);
            projs[n++] = i;
        }
    }
    return (n > 0 ? -1*projs[random(n)] : 0);
}

int mineral_cost(int fac, int prod) {
    if (prod >= 0)
        return tx_units[prod].cost * tx_cost_factor(fac, 1, -1);
    else
        return tx_facilities[-prod].cost * tx_cost_factor(fac, 1, -1);
}

int count_sea_tiles(int x, int y, int limit) {
    MAP* tile = mapsq(x, y);
    if (!tile || (tile->level >> 5) > LEVEL_SHORE_LINE) {
        return -1;
    }
    int n = 0;
    TileSearch ts;
    ts.init(x, y, WATER_ONLY);
    while (n < limit && (tile = ts.get_next()) != NULL) {
        n++;
    }
    debuglog("count_sea_tiles %d %d %d\n", x, y, n);
    return n;
}

bool switch_to_sea(int x, int y) {
    const int limit = 250;
    MAP* tile = mapsq(x, y);
    if (tile && tile->altitude < ALTITUDE_MIN_LAND) {
        return true;
    }
    int land = 0;
    int bases = 0;
    TileSearch ts;
    ts.init(x, y, LAND_ONLY);
    while (ts.visited() < limit && (tile = ts.get_next()) != NULL) {
        land++;
        if (tile->built_items & TERRA_BASE_IN_TILE)
            bases++;
    }
    debuglog("switch_to_sea %d %d %d %d\n", x, y, land, bases);
    return land / max(1, bases) < 12;
}

int bases_in_range(int x, int y, int range) {
    MAP* tile;
    int n = 0;
    int bases = 0;
    for (int i=-range*2; i<=range*2; i++) {
        for (int j=-range*2 + abs(i); j<=range*2 - abs(i); j+=2) {
            tile = mapsq(wrap(x + i, *tx_map_axis_x), y + j);
            n++;
            if (tile && tile->built_items & TERRA_BASE_IN_TILE)
                bases++;
        }
    }
    if (DEBUG) fprintf(debug_log, "bases_in_range %d %d %d %d %d\n", x, y, range, n, bases);
    return bases;
}

int consider_base(int id) {
    VEH* veh = &tx_vehicles[id];
    MAP* tile = mapsq(veh->x_coord, veh->y_coord);
    if (!tile || (tile->rocks & TILE_ROCKY) || (tile->built_items & BASE_DISALLOWED)
    || bases_in_range(veh->x_coord, veh->y_coord, 2) > 0
    || (unit_triad(veh->proto_id) == TRIAD_LAND && tile->altitude < ALTITUDE_MIN_LAND)
    || tile->altitude < 30)
        return tx_enemy_move(id);
    tx_action_build(id, 0);
    return SYNC;
}

int want_convoy(int fac, int x, int y, MAP* tile) {
    if (tile && tile->owner == fac) {
        int bonus = tx_bonus_at(x, y);
        if (tile->built_items & TERRA_CONDENSER)
            return RES_NUTRIENT;
        else if (tile->built_items & TERRA_MINE && tile->rocks & TILE_ROCKY)
            return RES_MINERAL;
        else if (tile->built_items & TERRA_FOREST && bonus == RES_NONE
        && !knows_tech(fac, TECH_EcoEng))
            return RES_MINERAL;
    }
    return RES_NONE;
}

int consider_convoy(int id) {
    VEH* veh = &tx_vehicles[id];
    MAP* tile = mapsq(veh->x_coord, veh->y_coord);
    int convoy = want_convoy(veh->faction_id, veh->x_coord, veh->y_coord, tile);
    if (convoy && convoy_not_used(veh->x_coord, veh->y_coord)) {
        veh->type_crawling = convoy-1;
        veh->move_status = STATUS_CONVOY;
        veh->target_action = 'C';
        return tx_veh_skip(id);
    }
    int i = 0;
    TileSearch ts;
    ts.init(veh->x_coord, veh->y_coord, LAND_ONLY);

    while (i++ < 30 && (tile = ts.get_next()) != NULL) {
        int other = unit_in_tile(tile);
        if (other >= 0 && tx_factions[veh->faction_id].diplo_status[other] & DIPLO_VENDETTA) {
            debuglog("convoy_skip %d %d %d %d %d %d\n", veh->x_coord, veh->y_coord,
                ts.cur_x, ts.cur_y, veh->faction_id, other);
            return tx_veh_skip(id);
        }
        convoy = want_convoy(veh->faction_id, ts.cur_x, ts.cur_y, tile);
        if (convoy && tx_can_convoy(ts.cur_x, ts.cur_y)) {
            veh->waypoint_1_x_coord = ts.cur_x;
            veh->waypoint_1_y_coord = ts.cur_y;
            veh->move_status = STATUS_GOTO;
            veh->target_action = 'G';
            return SYNC;
        }
    }
    return tx_veh_skip(id);
}

bool can_borehole(int x, int y, int bonus) {
    MAP* tile = mapsq(x, y);
    if (!tile || tile->built_items & BASE_DISALLOWED || bonus == RES_NUTRIENT)
        return false;
    if (bonus == RES_NONE && (tile->level & TILE_RAINY || tile->rocks & TILE_ROLLING))
        return false;
    if (bases_in_range(x, y, 2) < (bonus > RES_NUTRIENT ? 1 : 2))
        return false;
    int level = tile->level >> 5;
    for (int i=0; i<16; i+=2) {
        int x2 = wrap(x + offset[i], *tx_map_axis_x);
        int y2 = y + offset[i+1];
        tile = mapsq(x2, y2);
        if (!tile || tile->built_items & TERRA_THERMAL_BORE)
            return false;
        int level2 = tile->level >> 5;
        if (level2 < level && level2 > LEVEL_OCEAN_SHELF)
            return false;
    }
    return true;
}

int terraform_action(int id, int action, int flag) {
    VEH* veh = &tx_vehicles[id];
    MAP* sq = mapsq(veh->x_coord, veh->y_coord);

    if (conf.terraform_ai && veh->faction_id <= conf.factions_enabled
    && (1 << veh->faction_id) & ~(*tx_human_players) && sq) {
        if (sq->altitude < ALTITUDE_MIN_LAND)
            return tx_action_terraform(id, action, flag);
        bool rocky_sq = sq->rocks & TILE_ROCKY;
        bool rainy_sq = sq->level & TILE_RAINY;
        int bonus = tx_bonus_at(veh->x_coord, veh->y_coord);
        bool has_eco = knows_tech(veh->faction_id, TECH_EcoEng);

        if (has_eco && can_borehole(veh->x_coord, veh->y_coord, bonus)) {
            return tx_action_terraform(id, FORMER_THERMAL_BOREHOLE, flag);
        }
        if (!rocky_sq && sq->built_items & TERRA_CONDENSER && !(sq->built_items & TERRA_FARM))
            return tx_action_terraform(id, FORMER_FARM, flag);
        if (rocky_sq && bonus == RES_NUTRIENT)
            return tx_action_terraform(id, FORMER_LEVEL_TERRAIN, flag);
        if (sq->landmarks & 0x4) { // Jungle
            if (rocky_sq)
                return tx_action_terraform(id, FORMER_LEVEL_TERRAIN, flag);
            else if (action==FORMER_SOLAR_COLLECTOR)
                return tx_action_terraform(id, FORMER_FOREST, flag);
        } else if (rocky_sq && action==FORMER_SOLAR_COLLECTOR) {
            return tx_action_terraform(id, FORMER_MINE, flag);
        }
        if (action == FORMER_FARM && bonus != RES_NUTRIENT) {
            if (has_eco && rainy_sq)
                return tx_action_terraform(id, FORMER_FARM, flag);
            else
                return tx_action_terraform(id, FORMER_FOREST, flag);
        } else if (action == FORMER_CONDENSER && rocky_sq) {
            return tx_action_terraform(id, FORMER_MINE, flag);
        } else if (action == FORMER_MINE && !rocky_sq) {
            return tx_action_terraform(id, FORMER_FOREST, flag);
        } else if (action == FORMER_BUNKER) {
            if (rocky_sq)
                return tx_action_terraform(id, FORMER_MINE, flag);
            else
                return tx_action_terraform(id, FORMER_FOREST, flag);
        } else if (sq->built_items & TERRA_FARM) {
            if (has_eco && !(sq->built_items & TERRA_CONDENSER))
                return tx_action_terraform(id, FORMER_CONDENSER, flag);
        }
    }
    return tx_action_terraform(id, action, flag);
}

int find_proto(int fac, int triad, int mode, bool defend) {
    int basic = BSC_SCOUT_PATROL;
    debuglog("find_proto fac: %d triad: %d mode: %d def: %d\n", fac, triad, mode, defend);
    if (mode == WMODE_COLONIST)
        basic = BSC_COLONY_POD;
    else if (mode == WMODE_TERRAFORMER)
        basic = (triad == TRIAD_SEA ? BSC_SEA_FORMERS : BSC_FORMERS);
    else if (mode == WMODE_TRANSPORT)
        basic = BSC_TRANSPORT_FOIL;
    else if (mode == WMODE_INFOWAR)
        basic = BSC_PROBE_TEAM;
    int best = basic;
    for(int i=0; i<64; i++) {
        int id = fac*64 + i;
        UNIT* u = &tx_units[id];
        if (unit_triad(id) == triad && strlen(u->name) > 0) {
            if ((mode && u->weapon_mode != mode) || (!mode && u->weapon_type > WPN_STRING_DISRUPTOR)
            || (!mode && defend && u->chassis_type != CHSI_INFANTRY))
                continue;
            bool is_def = (tx_weapon[u->weapon_type].offense_value < tx_defense[u->armor_type].defense_value);
            if (best == basic) {
                best = id;
                debuglog("===> %s\n", (char*)&(tx_units[best].name));
            } else if (mode || (defend == is_def)) {
                int v1 = (defend ? tx_defense[u->armor_type].defense_value * u->reactor_type :
                          tx_weapon[u->weapon_type].offense_value * u->reactor_type);
                int v2 = (defend ?
                          tx_defense[tx_units[best].armor_type].defense_value * tx_units[best].reactor_type :
                          tx_weapon[tx_units[best].weapon_type].offense_value * tx_units[best].reactor_type);
                bool cheap = (defend && u->cost < tx_units[best].cost);
                if (v1 > v2 || (v1 == v2 && (cheap || random(3) == 0))) {
                    best = id;
                    debuglog("===> %s\n", (char*)&(tx_units[best].name));
                }
            }
        }
    }
    return best;
}

int find_facility(int base_id, int fac) {
    const int build_order[] = {
        FAC_RECYCLING_TANKS,
        FAC_RECREATION_COMMONS,
        FAC_CHILDREN_CRECHE,
        FAC_PERIMETER_DEFENSE,
        FAC_GENEJACK_FACTORY,
        FAC_NETWORK_NODE,
        FAC_TREE_FARM,
        FAC_HAB_COMPLEX,
        FAC_AEROSPACE_COMPLEX,
        FAC_COMMAND_CENTER,
        FAC_ENERGY_BANK,
        FAC_FUSION_LAB,
        FAC_HABITATION_DOME,
        FAC_RESEARCH_HOSPITAL
    };
    BASE* base = &tx_bases[base_id];
    Faction* faction = &tx_factions[fac];
    int minerals = base->mineral_surplus;
    int extra = base->minerals_accumulated/10;
    int threshold = min(10, 4 + *tx_current_turn/30);
    int hab_complex_limit = tx_basic->pop_limit_wo_hab_complex
        - tx_factions_meta[fac].rule_population;

    if (*tx_climate_future_change > 0) {
        MAP* tile = mapsq(base->x_coord, base->y_coord);
        if (tile && (tile->level >> 5) == LEVEL_SHORE_LINE && can_build(base_id, FAC_PRESSURE_DOME))
            return -FAC_PRESSURE_DOME;
    }
    if (base->drone_total > 0 && can_build(base_id, FAC_RECREATION_COMMONS))
        return -FAC_RECREATION_COMMONS;
    if (can_build(base_id, FAC_RECYCLING_TANKS))
        return -FAC_RECYCLING_TANKS;
    if (base->pop_size >= hab_complex_limit && can_build(base_id, FAC_HAB_COMPLEX))
        return -FAC_HAB_COMPLEX;
    if (minerals+extra >= threshold && project_capacity(fac)) {
        int proj = find_project(fac, 5*minerals + extra);
        if (proj != 0) return proj;
    }
    if (minerals+extra > threshold && has_facility(base_id, FAC_AEROSPACE_COMPLEX)) {
        if (faction->satellites_ODP < MAX_SAT && can_build(base_id, FAC_ORBITAL_DEFENSE_POD))
            return -FAC_ORBITAL_DEFENSE_POD;
        if (faction->satellites_mineral < MAX_SAT && can_build(base_id, FAC_NESSUS_MINING_STATION))
            return -FAC_NESSUS_MINING_STATION;
        if (faction->satellites_energy < MAX_SAT && can_build(base_id, FAC_ORBITAL_POWER_TRANS))
            return -FAC_ORBITAL_POWER_TRANS;
        if (faction->satellites_nutrient < MAX_SAT && can_build(base_id, FAC_SKY_HYDRO_LAB))
            return -FAC_SKY_HYDRO_LAB;
    }
    if (minerals >= 5 && find_hq(fac) < 0 && bases_in_range(base->x_coord, base->y_coord, 4) >= 5)
        return -FAC_HEADQUARTERS;

    for (int f : build_order) {
        if (f == FAC_CHILDREN_CRECHE && faction->SE_morale > 0)
            continue;
        if (can_build(base_id, f) && (tx_facilities[f].maint < 2 || *tx_current_turn > 40)) {
            return -1*f;
        }
    }
    if (knows_tech(fac, TECH_IndAuto))
        return BSC_SUPPLY_CRAWLER;
    return find_proto(fac, TRIAD_LAND, COMBAT, false);
}

int select_prod(int id) {
    BASE base = tx_bases[id];
    int owner = base.faction_id;
    int prod = base.queue_production_id[0];
    Faction* faction = &tx_factions[owner];

    if (prod < 0 && !can_build(id, abs(prod))) {
        if (DEBUG) fprintf(debug_log, "BUILD CHANGE\n");
        if (base.minerals_accumulated > 10)
            return find_facility(id, owner);
    } else if (prod < 0 || (base.minerals_accumulated > 10 && base_mins[id] < base.minerals_accumulated)) {
        if (DEBUG) fprintf(debug_log, "BUILD OLD\n");
        return prod;
    }

    int defenders = 0;
    int crawlers = 0;
    int formers = 0;
    int probes = 0;
    int pods = 0;
    int enemies = 0;
    int enemymask = 1;
    int enemyrange = 40;
    double enemymil = 0;
    double enemydist = 0;

    for (int i=1; i<8; i++) {
        if (i==owner || ~faction->diplo_status[i] & DIPLO_COMMLINK)
            continue;
        double mil = (1.0 * tx_factions[i].mil_strength_1)
            / max(1, faction->mil_strength_1);
        if (faction->diplo_status[i] & DIPLO_VENDETTA) {
            enemymask |= (1 << i);
            enemymil = max(enemymil, 1.0 * mil);
        } else if (~faction->diplo_status[i] & DIPLO_PACT) {
            enemymil = max(enemymil, 0.5 * mil);
        }
    }
    for (int i=0; i<*tx_total_num_bases; i++) {
        BASE* b = &tx_bases[i];
        if ((1 << b->faction_id) & enemymask) {
            enemyrange = min(enemyrange,
                map_range(base.x_coord, base.y_coord, b->x_coord, b->y_coord));
        }
    }
    enemymil = enemymil / sqrt(2 + enemyrange) * 3.0;

    for (int i=0; i<*tx_total_num_vehicles; i++) {
        VEH* veh = &tx_vehicles[i];
        UNIT* unit = &tx_units[veh->proto_id];
        if (veh->faction_id != owner && (1 << veh->faction_id) & ~enemymask)
            continue;
        if (veh->home_base_id == id) {
            if (unit->weapon_type == WPN_TERRAFORMING_UNIT)
                formers++;
            else if (unit->weapon_type == WPN_COLONY_MODULE)
                pods++;
            else if (unit->weapon_type == WPN_PROBE_TEAM)
                probes++;
            else if (unit->weapon_type == WPN_SUPPLY_TRANSPORT)
                crawlers++;
        }
        int range = map_range(base.x_coord, base.y_coord, veh->x_coord, veh->y_coord);

        if (veh->faction_id == owner && range <= 3) {
            if (unit_triad(veh->proto_id) == TRIAD_LAND && unit->weapon_type <= WPN_PSI_ATTACK) {
                if (range == 0)
                    defenders++;
                enemydist -= 0.2 / max(1, range);
            }
        } else if ((1 << veh->faction_id) & enemymask) {
            if (range <= 10) {
                enemydist += (veh->faction_id == 0 ? 0.8 : 1.0) / max(1, range);
                enemies++;
                //debuglog("enemy %d %d %d %d\n", veh->x_coord, veh->y_coord, veh->faction_id, range);
            }
        }
    }
    int has_ecology = knows_tech(owner, TECH_Ecology);
    int has_docflex = knows_tech(owner, TECH_DocFlex);
    int has_docair = knows_tech(owner, TECH_DocAir);
    int has_probes = knows_tech(owner, tx_weapon[WPN_PROBE_TEAM].preq_tech);
    int has_supply = knows_tech(owner, tx_weapon[WPN_SUPPLY_TRANSPORT].preq_tech);
    int minerals = base.mineral_surplus;
    bool can_build_ships = has_docflex && count_sea_tiles(base.x_coord, base.y_coord, 25) >= 25;
    bool land_area_full = switch_to_sea(base.x_coord, base.y_coord);
    bool build_pods = (base.pop_size > 1 || base.nutrient_surplus > 1)
        && pods < (*tx_current_turn < 60 ? 2 : 1)
        && faction->current_num_bases * 2 < min(80, *tx_map_area_sq_root);
    if (has_facility(id, FAC_PERIMETER_DEFENSE))
        enemydist *= 0.5;
    if (land_area_full)
        enemymil = max((faction->AI_fight * 0.2 + 0.5), enemymil);
    int mineral_cap = (enemyrange < 30 ? 0.6 : 0.4) * base.mineral_intake_1;
    double threat = 1 - (1 / (1 + max(0.0, enemymil) + max(0.0, enemydist)));

    debuglog("select_prod %d %d %d | %d %d %d %d %d %d %d | %d %d %.4f %.4f %.4f\n",
    *tx_current_turn, owner, id,
    defenders, formers, pods, crawlers, mineral_cap, build_pods, land_area_full,
    enemyrange, enemies, enemymil, enemydist, threat);

    if (defenders == 0 && (minerals > 2 || enemydist > 0.2)) {
        return find_proto(owner, TRIAD_LAND, COMBAT, true);
    } else if (minerals > 2 && base.mineral_consumption <= mineral_cap
    && random(100) < (int)(100.0 * threat)) {
        if (defenders > 2 && enemyrange < 12 && can_build(id, FAC_PERIMETER_DEFENSE))
            return -FAC_PERIMETER_DEFENSE;
        if (has_docair && faction->SE_police >= -3 && random(3) == 0)
            return find_proto(owner, TRIAD_AIR, COMBAT, false);
        else if (has_probes && probes < 1 && random(3) == 0)
            if (can_build_ships)
                return find_proto(owner, TRIAD_SEA, WMODE_INFOWAR, true);
            else
                return find_proto(owner, TRIAD_LAND, WMODE_INFOWAR, true);
        else if (water_base(id) || (can_build_ships && enemydist < 0.5 && random(3) == 0))
            if (random(3) == 0)
                return find_proto(owner, TRIAD_SEA, WMODE_TRANSPORT, true);
            else
                return find_proto(owner, TRIAD_SEA, COMBAT, false);
        else
            return find_proto(owner, TRIAD_LAND, COMBAT, false);
    } else if (has_ecology && formers <= min(2, base.pop_size/3)) {
        if (water_base(id))
            return find_proto(owner, TRIAD_SEA, WMODE_TERRAFORMER, true);
        else
            return find_proto(owner, TRIAD_LAND, WMODE_TERRAFORMER, true);
    } else {
        if (has_supply && crawlers <= min(2, base.pop_size/3) && !water_base(id))
            return BSC_SUPPLY_CRAWLER;
        if (build_pods && !can_build(id, FAC_RECYCLING_TANKS))
            if (can_build_ships && land_area_full)
                return find_proto(owner, TRIAD_SEA, WMODE_COLONIST, true);
            else
                return BSC_COLONY_POD;
        else
            return find_facility(id, owner);
    }
}








