#pragma once
#include <stdbool.h>

// 도감 해금
bool save_is_plant_completed(const char* plant_id);     // 도감 씬에서 사용
bool save_mark_plant_completed(const char* plant_id);   // 최종 성장 시 호출

// 기록 추가 (Codex가 읽는 형식)
bool log_water(const char* plant_id, int amount_ml);                 // 물 주기
bool log_sun(const char* plant_id, int minutes, int ppfd);           // 햇빛
bool log_stage(const char* plant_id, const char* stage_name);        // 단계 전환
bool log_window(const char* plant_id, bool open);                    // 창문 상태

// Codex가 읽을 때 쓸 수 있는 단순 구조(선택)
typedef struct {
    char ts[32];     // "YYYY-MM-DD HH:MM"
    char line[160];  // 표시용 문자열 (옵션)
} CodexLog;

// 도감에서 로그 읽어오기 (이미 구현했다면 그대로 사용)
int save_get_plant_logs(const char* plant_id, CodexLog* out, int max);
