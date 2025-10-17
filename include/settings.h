#pragma once
#include <stdbool.h>

typedef struct {
    float bgm_volume;     // 0~1
    float sfx_volume;     // 0~1
    bool  mute_bgm;
    bool  mute_sfx;
} AudioSettings;

typedef struct {
    int   width, height;
    bool  fullscreen;
} VideoSettings;

typedef struct {
    char  last_selected_plant[64]; // id string (예: "monstera")
    bool  window_open;
} GameplaySettings;

typedef struct {
    int key_toggle_window; // SDL_Scancode 또는 SDL_Keycode
    int key_water;
} ControlSettings;

typedef struct {
    AudioSettings    audio;
    VideoSettings    video;
    GameplaySettings gameplay;
    ControlSettings  controls;
} GameSettings;

extern GameSettings G_Settings;

void settings_set_defaults(void);

// 설정 파일 경로 얻기 (SDL_GetPrefPath 사용)
int  settings_get_path(char* out, int outsz);

// 로드/세이브
// 반환: 1 성공, 0 실패
int  settings_load(void);             // pref path의 settings.json
int  settings_save(void);             // pref path의 settings.json

// 즉시 적용 (오디오/창 모드 등)
void settings_apply_audio(void);
