#ifndef CORE_H
#define CORE_H
#include "common.h"

// 이후 plant/sim/stats/save 등을 여기에서 한 번에 include 예정
#endif

// plant_db
typedef struct {
    char id[32];
    char name_kr[64];
    char latin[64];
    int  light_level;     // 0(약)~2(강)
    int  water_days;      // 권장 급수 주기(일)
    int  min_temp, max_temp;
    char icon_path[128];  // 아이콘(선택)
} PlantInfo;

int       plantdb_load(const char* json_path);  // returns count or -1
void      plantdb_free(void);
int       plantdb_count(void);
const PlantInfo* plantdb_get(int idx);