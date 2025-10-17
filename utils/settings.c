#define _CRT_SECURE_NO_WARNINGS
#include "../include/settings.h"
#include <SDL2/SDL.h>
#include <parson.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "../game.h"       // G_Renderer, G_Window, G_SFX_Click, etc.
#include <SDL2/SDL_mixer.h>  // Mix_VolumeMusic, Mix_VolumeChunk

GameSettings G_Settings;

static float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

void settings_set_defaults(void) {
    memset(&G_Settings, 0, sizeof(G_Settings));
    G_Settings.audio.bgm_volume = 0.6f;
    G_Settings.audio.sfx_volume = 0.8f;
    G_Settings.audio.mute_bgm = false;
    G_Settings.audio.mute_sfx = false;

    G_Settings.video.width = 1920;
    G_Settings.video.height = 1080;
    G_Settings.video.fullscreen = false;

    SDL_strlcpy(G_Settings.gameplay.last_selected_plant, "", sizeof(G_Settings.gameplay.last_selected_plant));
    G_Settings.gameplay.window_open = false;

    // 키 코드는 SDL_Keycode 사용 예시
    G_Settings.controls.key_toggle_window = SDLK_w;
    G_Settings.controls.key_water = SDLK_SPACE;
}

int settings_get_path(char* out, int outsz) {
    char* base = SDL_GetPrefPath("DOLSUI", "GROWING");
    if (!base) return 0;
    SDL_snprintf(out, outsz, "%ssettings.json", base);
    SDL_free(base);
    return 1;
}

static int settings_load_from_path(const char* path) {
    JSON_Value* root = json_parse_file(path);
    if (!root) return 0;
    JSON_Object* obj = json_value_get_object(root);
    if (!obj) { json_value_free(root); return 0; }

    // audio
    JSON_Object* audio = json_object_get_object(obj, "audio");
    if (audio) {
        G_Settings.audio.bgm_volume = clamp01((float)json_object_get_number(audio, "bgm_volume"));
        G_Settings.audio.sfx_volume = clamp01((float)json_object_get_number(audio, "sfx_volume"));
        G_Settings.audio.mute_bgm = json_object_get_boolean(audio, "mute_bgm") == 1;
        G_Settings.audio.mute_sfx = json_object_get_boolean(audio, "mute_sfx") == 1;
    }

    // video
    JSON_Object* video = json_object_get_object(obj, "video");
    if (video) {
        G_Settings.video.width = (int)json_object_get_number(video, "width");
        G_Settings.video.height = (int)json_object_get_number(video, "height");
        G_Settings.video.fullscreen = json_object_get_boolean(video, "fullscreen") == 1;
    }

    // gameplay
    JSON_Object* gp = json_object_get_object(obj, "gameplay");
    if (gp) {
        const char* last = json_object_get_string(gp, "last_selected_plant");
        if (last) SDL_strlcpy(G_Settings.gameplay.last_selected_plant, last, sizeof(G_Settings.gameplay.last_selected_plant));
        G_Settings.gameplay.window_open = json_object_get_boolean(gp, "window_open") == 1;
    }

    // controls
    JSON_Object* ctrl = json_object_get_object(obj, "controls");
    if (ctrl) {
        // 키는 문자열로 저장했다면 매핑 로직 필요. 여기선 정수로 저장했다고 가정.
        G_Settings.controls.key_toggle_window = (int)json_object_get_number(ctrl, "key_toggle_window");
        G_Settings.controls.key_water = (int)json_object_get_number(ctrl, "key_water");
    }

    json_value_free(root);
    return 1;
}

int settings_load(void) {
    settings_set_defaults();

    char path[1024];
    if (!settings_get_path(path, sizeof(path))) return 0;

    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    if (!rw) {
        SDL_Log("[SETTINGS] no file, using defaults. path=%s", path);
        return 1;
    }
    SDL_RWclose(rw);

    if (!settings_load_from_path(path)) {
        SDL_Log("[SETTINGS] parse failed, keep defaults. path=%s", path);
        return 0; // 실패를 알려도 되고 1로 해도 됨(기본값 유지)
    }
    SDL_Log("[SETTINGS] loaded from %s (bgm=%.2f sfx=%.2f muteB=%d muteS=%d)",
        path, G_Settings.audio.bgm_volume, G_Settings.audio.sfx_volume,
        G_Settings.audio.mute_bgm, G_Settings.audio.mute_sfx);
    return 1;
}

static int settings_save_to_path(const char* path) {
    JSON_Value* root = json_value_init_object();
    JSON_Object* obj = json_value_get_object(root);

    // --- audio ---
    {
        JSON_Value* v = json_value_init_object();
        JSON_Object* o = json_value_get_object(v);
        json_object_set_number(o, "bgm_volume", G_Settings.audio.bgm_volume);
        SDL_Log("%.2f 값 저장됨", G_Settings.audio.bgm_volume);
        json_object_set_number(o, "sfx_volume", G_Settings.audio.sfx_volume);
        json_object_set_boolean(o, "mute_bgm", G_Settings.audio.mute_bgm);
        json_object_set_boolean(o, "mute_sfx", G_Settings.audio.mute_sfx);
        json_object_set_value(obj, "audio", v);
    }
    // --- video ---
    {
        JSON_Value* v = json_value_init_object();
        JSON_Object* o = json_value_get_object(v);
        json_object_set_number(o, "width", G_Settings.video.width);
        json_object_set_number(o, "height", G_Settings.video.height);
        json_object_set_boolean(o, "fullscreen", G_Settings.video.fullscreen);
        json_object_set_value(obj, "video", v);
    }
    // --- gameplay ---
    {
        JSON_Value* v = json_value_init_object();
        JSON_Object* o = json_value_get_object(v);
        json_object_set_string(o, "last_selected_plant", G_Settings.gameplay.last_selected_plant);
        json_object_set_boolean(o, "window_open", G_Settings.gameplay.window_open);
        json_object_set_value(obj, "gameplay", v);
    }
    // --- controls ---
    {
        JSON_Value* v = json_value_init_object();
        JSON_Object* o = json_value_get_object(v);
        json_object_set_number(o, "key_toggle_window", G_Settings.controls.key_toggle_window);
        json_object_set_number(o, "key_water", G_Settings.controls.key_water);
        json_object_set_value(obj, "controls", v);
    }

    // 실제 저장
    int rc = json_serialize_to_file_pretty(root, path);
    json_value_free(root);

    if (rc != JSONSuccess) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[SETTINGS] json_serialize_to_file_pretty FAILED path=%s (rc=%d, errno=%d:%s)",
            path, rc, errno, strerror(errno));
        return 0;
    }

    // 저장 확인 (파일 존재 여부)
    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    if (!rw) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[SETTINGS] save supposedly ok but file not found path=%s (errno=%d:%s)",
            path, errno, strerror(errno));
        return 0;
    }
    SDL_RWclose(rw);

    SDL_Log("[SETTINGS] saved ok: %s", path);
    return 1;
}

int settings_save(void) {
    char path[1024];
    if (!settings_get_path(path, sizeof(path))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[SETTINGS] get_path failed");
        return 0;
    }
    SDL_Log("[SETTINGS] saving to %s", path);
    return settings_save_to_path(path);
}

void settings_apply_audio(void) {
    // 0~1 → 0~128 변환
    int bgm = (int)(clamp01(G_Settings.audio.bgm_volume) * MIX_MAX_VOLUME);
    int sfx = (int)(clamp01(G_Settings.audio.sfx_volume) * MIX_MAX_VOLUME);
    if (G_Settings.audio.mute_bgm) bgm = 0;
    if (G_Settings.audio.mute_sfx) sfx = 0;

    Mix_VolumeMusic(bgm);
    // 전역 SFX(예: 버튼 클릭 소리)에 곱하기
    if (G_SFX_Click) Mix_VolumeChunk(G_SFX_Click, sfx);
    // 다른 효과음들도 필요하면 동일 처리
}
