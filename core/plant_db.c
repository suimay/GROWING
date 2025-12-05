#include "../include/common.h"
#include "../include/core.h"
#include <parson.h>   // vcpkg or local header


static PlantInfo* g_plants = NULL;
static int g_count = 0;

void plantdb_free(void) {
    if (g_plants) { SDL_free(g_plants); g_plants = NULL; }
    g_count = 0;
}

int plantdb_count(void) { return g_count; }
const PlantInfo* plantdb_get(int idx) {
    if (idx < 0 || idx >= g_count) return NULL;
    return &g_plants[idx];
}

int plantdb_load(const char* json_path) {
    plantdb_free();

    JSON_Value* root = json_parse_file(json_path);
    if (!root) { SDL_Log("plantdb: JSON open fail: %s", json_path); return -1; }

    JSON_Array* arr = json_value_get_array(root);
    if (!arr) { SDL_Log("plantdb: not array root"); json_value_free(root); return -1; }

    g_count = (int)json_array_get_count(arr);
    if (g_count <= 0) { json_value_free(root); return 0; }

    g_plants = (PlantInfo*)SDL_calloc(g_count, sizeof(PlantInfo));
    if (!g_plants) { json_value_free(root); return -1; }

    for (int i = 0;i < g_count;i++) {
        JSON_Object* o = json_array_get_object(arr, i);
        const char* id = json_object_get_string(o, "id");
        const char* namek = json_object_get_string(o, "name_kr");
        const char* latin = json_object_get_string(o, "latin");
        const char* icon = json_object_get_string(o, "icon");
        int light = (int)json_object_get_number(o, "light_level");
        int water = (int)json_object_get_number(o, "water_days");
        int tmin = (int)json_object_get_number(o, "min_temp");
        int tmax = (int)json_object_get_number(o, "max_temp");

        PlantInfo* p = &g_plants[i];
        SDL_strlcpy(p->id, id ? id : "", sizeof(p->id));
        SDL_strlcpy(p->name_kr, namek ? namek : "", sizeof(p->name_kr));
        SDL_strlcpy(p->latin, latin ? latin : "", sizeof(p->latin));
        SDL_strlcpy(p->icon_path, icon ? icon : "", sizeof(p->icon_path));
        p->light_level = light;
        p->water_days = water;
        p->min_temp = tmin;
        p->max_temp = tmax;
    }

    json_value_free(root);
    SDL_Log("plantdb: loaded %d plants", g_count);
    return g_count;
}

int plantdb_find_index_by_id(const char* id) {
    if (!id || !*id) return -1; // 빈 문자열 방어
    int n = plantdb_count();    // 전체 식물 개수
    for (int i = 0; i < n; ++i) {
        const PlantInfo* p = plantdb_get(i);
        if (p && SDL_strcmp(p->id, id) == 0) {
            return i; // ✅ 일치하는 인덱스 반환
        }
    }
    return -1; // 찾지 못함
}

