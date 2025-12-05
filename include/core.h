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

    float moisture_opt;
    float temp_min;
    float temp_max;

    float humidity_min;
    float humidity_max;
} PlantInfo;

typedef struct PlantStatus {
    float moisture;    // 수분 (0~100)
    float temp;        // 방 온도와 거의 같게 움직이게 할 수도 있음
    float temp_min;
    float temp_max;

    float humidity_min;
    float humidity_max;
    float humidity;    // 습도 (0~100)
    float light;       // 일조량 (나중에 구현)
    float happiness;   // 행복도 (0~100)
    float nutrition;   // 영양 (0~100)
} PlantStatus;

int       plantdb_load(const char* json_path);  // returns count or -1
void      plantdb_free(void);
int       plantdb_count(void);
int plantdb_find_index_by_id(const char* id);
const PlantInfo* plantdb_get(int idx);

extern PlantStatus s_status;