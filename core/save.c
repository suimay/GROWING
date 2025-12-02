#define _CRT_SECURE_NO_WARNINGS
#include "../include/save.h"
#include <SDL2/SDL.h>
#include <parson.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

// ---------- 경로 도우미 ----------
static int get_base_dir(char* out, int outsz) {
    char* base = SDL_GetPrefPath("DOLSUI", "GROWING");
    if (!base) return 0;
    SDL_strlcpy(out, base, outsz);
    SDL_free(base);
    return 1;
}

static int ensure_logs_dir(char* out_logs, int outsz) {
    char base[1024];
    if (!get_base_dir(base, sizeof(base))) return 0;
    SDL_snprintf(out_logs, outsz, "%slogs", base);

    // 간단 mkdir (이미 있으면 실패해도 무시)
    MKDIR(out_logs);
    return 1;
}

static int get_codex_json_path(char* out, int outsz) {
    char base[1024];
    if (!get_base_dir(base, sizeof(base))) return 0;
    SDL_snprintf(out, outsz, "%scodex.json", base);
    return 1;
}

static int get_log_json_path(const char* plant_id, char* out, int outsz) {
    char logs[1024];
    if (!ensure_logs_dir(logs, sizeof(logs))) return 0;
    SDL_snprintf(out, outsz, "%s/%s.json", logs, plant_id);
    return 1;
}

// ---------- 시간 문자열 ----------
static void now_ts(char* out, int outsz) {
    time_t t = time(NULL);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    strftime(out, outsz, "%Y-%m-%d %H:%M", &lt);
}

// ---------- codex.json: 해금 ----------
bool save_is_plant_completed(const char* plant_id) {
    char path[1024];
    if (!get_codex_json_path(path, sizeof(path))) return false;

    JSON_Value* root = json_parse_file(path);
    if (!root) return false;
    JSON_Object* obj = json_value_get_object(root);
    JSON_Array* arr = json_object_get_array(obj, "unlocked");
    if (!arr) { json_value_free(root); return false; }

    size_t n = json_array_get_count(arr);
    for (size_t i = 0; i < n; ++i) {
        const char* s = json_array_get_string(arr, i);
        if (s && SDL_strcmp(s, plant_id) == 0) {
            json_value_free(root);
            return true;
        }
    }
    json_value_free(root);
    return false;
}

bool save_mark_plant_completed(const char* plant_id) {
    char path[1024];
    if (!get_codex_json_path(path, sizeof(path))) return false;

    JSON_Value* root = json_parse_file(path);
    JSON_Object* obj = NULL;
    JSON_Array* arr = NULL;

    if (!root) {
        root = json_value_init_object();
        obj = json_value_get_object(root);
        JSON_Value* arrv = json_value_init_array();
        json_object_set_value(obj, "unlocked", arrv);
        arr = json_object_get_array(obj, "unlocked");
    }
    else {
        obj = json_value_get_object(root);
        arr = json_object_get_array(obj, "unlocked");
        if (!arr) {
            JSON_Value* v = json_value_init_array();
            json_object_set_value(obj, "unlocked", v);
            arr = json_object_get_array(obj, "unlocked");
        }
    }

    // 이미 있으면 패스
    size_t n = json_array_get_count(arr);
    for (size_t i = 0; i < n; ++i) {
        const char* s = json_array_get_string(arr, i);
        if (s && SDL_strcmp(s, plant_id) == 0) {
            json_serialize_to_file_pretty(root, path);
            json_value_free(root);
            return true;
        }
    }
    // 추가
    json_array_append_string(arr, plant_id);
    json_serialize_to_file_pretty(root, path);
    json_value_free(root);
    return true;
}

// ---------- logs/<id>.json: 기록 추가 ----------
static bool append_log_obj(const char* plant_id, JSON_Object* log_obj) {
    char path[1024];
    if (!get_log_json_path(plant_id, path, sizeof(path))) return false;

    JSON_Value* root = json_parse_file(path);
    JSON_Object* obj = NULL;
    JSON_Array* history = NULL;

    if (!root) {
        root = json_value_init_object();
        obj = json_value_get_object(root);
        json_object_set_string(obj, "plant_id", plant_id);
        JSON_Value* arrv = json_value_init_array();
        json_object_set_value(obj, "history", arrv);
        history = json_object_get_array(obj, "history");
    }
    else {
        obj = json_value_get_object(root);
        history = json_object_get_array(obj, "history");
        if (!history) {
            JSON_Value* arrv = json_value_init_array();
            json_object_set_value(obj, "history", arrv);
            history = json_object_get_array(obj, "history");
        }
    }

    JSON_Value* entry = json_value_init_object();
    JSON_Object* eobj = json_value_get_object(entry);

    // 공통 ts,event 복사
    const char* ts = json_object_get_string(log_obj, "ts");
    const char* evt = json_object_get_string(log_obj, "event");
    if (ts)  json_object_set_string(eobj, "ts", ts);
    if (evt) json_object_set_string(eobj, "event", evt);

    // 나머지 필드 복사 (간단하게 필요한 키만 체크)
    if (json_object_has_value_of_type(log_obj, "amount_ml", JSONNumber))
        json_object_set_number(eobj, "amount_ml", json_object_get_number(log_obj, "amount_ml"));

    if (json_object_has_value_of_type(log_obj, "minutes", JSONNumber))
        json_object_set_number(eobj, "minutes", json_object_get_number(log_obj, "minutes"));

    if (json_object_has_value_of_type(log_obj, "ppfd", JSONNumber))
        json_object_set_number(eobj, "ppfd", json_object_get_number(log_obj, "ppfd"));

    if (json_object_has_value_of_type(log_obj, "value", JSONString))
        json_object_set_string(eobj, "value", json_object_get_string(log_obj, "value"));

    if (json_object_has_value_of_type(log_obj, "open", JSONBoolean))
        json_object_set_boolean(eobj, "open", json_object_get_boolean(log_obj, "open"));

    json_array_append_value(history, entry);
    json_serialize_to_file_pretty(root, path);
    json_value_free(root);
    return true;
}

// ——— 공개 API 구현 ———
static void fill_minutes_ppfd(JSON_Object* o, int minutes, int ppfd) {
    json_object_set_number(o, "minutes", minutes);
    json_object_set_number(o, "ppfd", ppfd);
}
static void fill_amount(JSON_Object* o, int amount_ml) {
    json_object_set_number(o, "amount_ml", amount_ml);
}
static void fill_stage(JSON_Object* o, const char* stage_name) {
    json_object_set_string(o, "value", stage_name ? stage_name : "");
}
static void fill_open(JSON_Object* o, bool open) {
    json_object_set_boolean(o, "open", open ? 1 : 0);
}

static bool add_log_water(const char* plant_id, int amount_ml) {
    char ts[32]; now_ts(ts, sizeof(ts));
    JSON_Value* v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_string(o, "ts", ts);
    json_object_set_string(o, "event", "water");
    fill_amount(o, amount_ml);
    bool ok = append_log_obj(plant_id, o);
    json_value_free(v);
    return ok;
}
static bool add_log_sun(const char* plant_id, int minutes, int ppfd) {
    char ts[32]; now_ts(ts, sizeof(ts));
    JSON_Value* v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_string(o, "ts", ts);
    json_object_set_string(o, "event", "sun");
    fill_minutes_ppfd(o, minutes, ppfd);
    bool ok = append_log_obj(plant_id, o);
    json_value_free(v);
    return ok;
}
static bool add_log_stage(const char* plant_id, const char* stage_name) {
    char ts[32]; now_ts(ts, sizeof(ts));
    JSON_Value* v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_string(o, "ts", ts);
    json_object_set_string(o, "event", "stage");
    fill_stage(o, stage_name);
    bool ok = append_log_obj(plant_id, o);
    json_value_free(v);
    return ok;
}
static bool add_log_window(const char* plant_id, bool open) {
    char ts[32]; now_ts(ts, sizeof(ts));
    JSON_Value* v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_string(o, "ts", ts);
    json_object_set_string(o, "event", "window");
    fill_open(o, open);
    bool ok = append_log_obj(plant_id, o);
    json_value_free(v);
    return ok;
}

// 공개 함수
bool log_water(const char* plant_id, int amount_ml) { return add_log_water(plant_id, amount_ml); }
bool log_sun(const char* plant_id, int minutes, int ppfd) { return add_log_sun(plant_id, minutes, ppfd); }
bool log_stage(const char* plant_id, const char* stage_name) { return add_log_stage(plant_id, stage_name); }
bool log_window(const char* plant_id, bool open) { return add_log_window(plant_id, open); }

// (선택) Codex에 보여주기 위한 간단 변환
int save_get_plant_logs(const char* plant_id, CodexLog* out, int max) {
    if (!out || max <= 0) return 0;
    char path[1024];
    if (!get_log_json_path(plant_id, path, sizeof(path))) return 0;

    JSON_Value* root = json_parse_file(path);
    if (!root) return 0;
    JSON_Object* obj = json_value_get_object(root);
    JSON_Array* history = json_object_get_array(obj, "history");
    if (!history) { json_value_free(root); return 0; }

    int n = (int)json_array_get_count(history);
    if (n > max) n = max;

    for (int i = 0; i < n; ++i) {
        JSON_Object* e = json_array_get_object(history, i);
        const char* ts = json_object_get_string(e, "ts");
        const char* ev = json_object_get_string(e, "event");
        SDL_strlcpy(out[i].ts, ts ? ts : "", sizeof(out[i].ts));

        // 라인 만들기(간단 버전)
        if (ev && SDL_strcmp(ev, "water") == 0) {
            int ml = (int)json_object_get_number(e, "amount_ml");
            SDL_snprintf(out[i].line, sizeof(out[i].line), "[%s] 물주기 %dml", out[i].ts, ml);
        }
        else if (ev && SDL_strcmp(ev, "sun") == 0) {
            int m = (int)json_object_get_number(e, "minutes");
            int p = (int)json_object_get_number(e, "ppfd");
            SDL_snprintf(out[i].line, sizeof(out[i].line), "[%s] 햇빛 %d분 (PPFD %d)", out[i].ts, m, p);
        }
        else if (ev && SDL_strcmp(ev, "stage") == 0) {
            const char* v = json_object_get_string(e, "value");
            SDL_snprintf(out[i].line, sizeof(out[i].line), "[%s] 단계 전환: %s", out[i].ts, v ? v : "");
        }
        else if (ev && SDL_strcmp(ev, "window") == 0) {
            int open = json_object_get_boolean(e, "open") == 1;
            SDL_snprintf(out[i].line, sizeof(out[i].line), "[%s] 창문 %s", out[i].ts, open ? "열림" : "닫힘");
        }
        else {
            SDL_snprintf(out[i].line, sizeof(out[i].line), "[%s] %s", out[i].ts, ev ? ev : "(unknown)");
        }
    }
    json_value_free(root);
    return n;
}
