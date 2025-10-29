// scenes/scene_gameplay.c
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"       // PlantInfo, plantdb_get
#include "../include/save.h"       // log_water, log_window
#include "../include/settings.h"   // settings_apply_audio
#include "../include/gameplay.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <parson.h>"

LoadCtx s_loadCtx;          // 정의(여기 딱 1곳)

// -----------------------------
// 상태
extern void gameplay_ensure_bgm_loaded(void);
extern void load_background_animation(void);
extern void settings_apply_audio(void);

// 로딩 단계 상태


static int s_step = 0;  // 0부터 차례로 증가
// -----------------------------
static const PlantInfo* s_plant = NULL;

static UIButton s_btnBack;
static UIButton s_btnWater;
static UIButton s_btnWindow;

static int  s_waterCount = 0;
static bool s_windowOpen = false;

static SDL_Texture* s_bgTexture = NULL;   // fallback 배경(옵션)
static SDL_Texture* s_backIcon = NULL;   // ← 버튼 아이콘(옵션)

// -----------------------------
// 인게임 BGM (랜덤 재생)
// -----------------------------


int gameplay_loading_job(void* u, float* out_p) {
    LoadCtx* c = (LoadCtx*)u;
    if (!c || !out_p) return 1; // 방어

    Uint32 now = SDL_GetTicks();

    switch (c->step) {
    case 0:
        *out_p = 5.f;
        c->t0 = now;
        c->step = 1;
        return 0;

    case 1:
        // 약간의 페이크 대기(로딩 연출)
        if (now - c->t0 < 120) { *out_p = 35.f; return 0; }
        // 실제 비싼 작업들
        load_background_animation();  // 아틀라스/프레임 파싱
        *out_p = 75.f;
        c->step = 2;
        return 0;

    case 2:
        gameplay_ensure_bgm_loaded(); // 인게임 BGM 로드(중복로드 가드됨)
        *out_p = 95.f;
        c->step = 99;
        return 0;

    case 99:
        *out_p = 100.f;
        return 1; // ✅ 완료 신호
    }
    return 0;
}



static const char* kGameBGMs[] = {
    ASSETS_SOUNDS_DIR "PLAYSCENEBGM1.wav",
    ASSETS_SOUNDS_DIR "Snowy_winter.wav",
    ASSETS_SOUNDS_DIR "In_the_fairytale.wav",
    ASSETS_SOUNDS_DIR "To_School_at_Sunrise.wav",
    ASSETS_SOUNDS_DIR "Living_in_the_grass.wav",
};
static Mix_Music* s_gameBGMs[SDL_arraysize(kGameBGMs)] = { 0 };
static int s_bgmLoaded = 0;
static int s_lastBgm = -1;

// -----------------------------
// 배경 애니메이션(아틀라스)
// -----------------------------
typedef struct {
    SDL_Rect rect;
    int      duration_ms;
    SDL_Texture* texture; // 아틀라스 모드에서는 NULL, fallback 분할텍스쳐 모드에서만 사용
} AnimFrame;

static SDL_Texture* s_bgAtlas = NULL;
static AnimFrame    s_bgFrames[128];
static int          s_bgFrameCount = 0;
static int          s_bgFrameIndex = 0;
static float        s_bgFrameElapsedMs = 0.f;
static bool         s_bgFramesUseAtlas = false;

// -----------------------------
// 유틸
// -----------------------------
static void draw_text(SDL_Renderer* r, SDL_Color color, int x, int y, const char* fmt, ...)
{
    if (!G_FontMain || !fmt) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_Surface* surf = TTF_RenderUTF8_Blended(G_FontMain, buf, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void destroy_frame_textures(void)
{
    for (int i = 0; i < (int)SDL_arraysize(s_bgFrames); ++i) {
        if (s_bgFrames[i].texture) {
            SDL_DestroyTexture(s_bgFrames[i].texture);
            s_bgFrames[i].texture = NULL;
        }
    }
}

static void gameplay_ensure_bgm_loaded(void)
{
    if (s_bgmLoaded) return;

    for (int i = 0; i < (int)SDL_arraysize(kGameBGMs); ++i) {
        s_gameBGMs[i] = Mix_LoadMUS(kGameBGMs[i]);
        if (!s_gameBGMs[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[GAMEPLAY BGM] Load fail %s : %s", kGameBGMs[i], Mix_GetError());
        }
        else {
            SDL_Log("[GAMEPLAY BGM] loaded: %s", kGameBGMs[i]);
        }
    }
    s_bgmLoaded = 1;

    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
}

static int pick_random_index(void)
{
    int n = (int)SDL_arraysize(kGameBGMs);
    int tries = 10;
    while (tries--) {
        int r = rand() % n;
        if (r != s_lastBgm) return r;
    }
    return (s_lastBgm + 1) % n;
}

static void SDLCALL gameplay_on_music_finished(void)
{
    int idx = pick_random_index();
    s_lastBgm = idx;
    if (s_gameBGMs[idx]) {
        if (Mix_FadeInMusic(s_gameBGMs[idx], /*loops=*/1, /*ms=*/200) < 0) {
            SDL_Log("[GAMEPLAY BGM] FadeIn error: %s", Mix_GetError());
        }
    }
}

// -----------------------------
// 버튼 콜백
// -----------------------------
static void on_back(void* ud)
{
    (void)ud;
    scene_switch(SCENE_MAINMENU);
}

static void on_water(void* ud)
{
    (void)ud;
    s_waterCount++;
    if (s_plant) log_water(s_plant->id, 120); // 임시 120ml
}

static void on_window(void* ud)
{
    (void)ud;
    s_windowOpen = !s_windowOpen;

    // 버튼 텍스트 갱신
    ui_button_init(&s_btnWindow, s_btnWindow.r, s_windowOpen ? "창문 닫기" : "창문 열기");
    ui_button_set_callback(&s_btnWindow, on_window, NULL);
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);

    SDL_Log(s_windowOpen ? "[GAME] window open." : "[GAME] window close.");
    if (s_plant) log_window(s_plant->id, s_windowOpen);
}

// -----------------------------
// 레이아웃
// -----------------------------
static void layout(void)
{
    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);

    const int margin = 40;
    const int backSize = 72;
    const int actionWidth = 200;
    const int actionHeight = 68;
    const int actionGap = 24;

    ui_button_init(&s_btnBack, (SDL_Rect) { margin, margin, backSize, backSize }, "");
    ui_button_init(&s_btnWater, (SDL_Rect) { margin, h - actionHeight - margin, actionWidth, actionHeight }, "물 주기");
    ui_button_init(&s_btnWindow, (SDL_Rect) { margin + actionWidth + actionGap, h - actionHeight - margin, actionWidth, actionHeight }, "창문 열기");

    ui_button_set_callback(&s_btnBack, on_back, NULL);
    ui_button_set_callback(&s_btnWater, on_water, NULL);
    ui_button_set_callback(&s_btnWindow, on_window, NULL);

    ui_button_set_sfx(&s_btnBack, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWater, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);
}

// -----------------------------
// 배경 애니메이션 로드
// (Aseprite JSON: frames 가 object 또는 array 인 경우 모두 지원)
// -----------------------------
static void load_background_animation(void)
{
    destroy_frame_textures();
    s_bgFrameCount = 0;
    s_bgFrameIndex = 0;
    s_bgFrameElapsedMs = 0.f;
    s_bgFramesUseAtlas = false;

    if (s_bgAtlas) { SDL_DestroyTexture(s_bgAtlas); s_bgAtlas = NULL; }

    s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "garagame-Sheet.png");
    if (!s_bgAtlas) {
        SDL_Log("[ANIM] Load garagame-Sheet.png failed: %s", IMG_GetError());
    }

    JSON_Value* root = json_parse_file(ASSETS_DIR "data/garagame.json");
    if (!root) { SDL_Log("[ANIM] parse fail"); return; }
    JSON_Object* robj = json_value_get_object(root);

    JSON_Object* framesObj = json_object_get_object(robj, "frames");
    JSON_Array* framesArr = json_object_get_array(robj, "frames");

    int atlasW = 0, atlasH = 0;
    if (s_bgAtlas) SDL_QueryTexture(s_bgAtlas, NULL, NULL, &atlasW, &atlasH);

    // frames as object
    if (framesObj) {
        size_t count = json_object_get_count(framesObj);
        SDL_Log("[ANIM] frames=object, count=%zu", count);
        for (size_t idx = 0; idx < count; ++idx) {
            const char* key = json_object_get_name(framesObj, idx);
            JSON_Object* fobj = json_object_get_object(framesObj, key);
            if (!fobj) continue;

            JSON_Object* rect = json_object_get_object(fobj, "frame");
            if (!rect) { SDL_Log("[ANIM] missing frame for key=%s", key); continue; }

            AnimFrame fr = { 0 };
            fr.rect.x = (int)json_object_get_number(rect, "x");
            fr.rect.y = (int)json_object_get_number(rect, "y");
            fr.rect.w = (int)json_object_get_number(rect, "w");
            fr.rect.h = (int)json_object_get_number(rect, "h");
            fr.duration_ms = (int)json_object_get_number(fobj, "duration");
            if (fr.duration_ms <= 0) fr.duration_ms = 100;

            if (fr.rect.w <= 0 || fr.rect.h <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] invalid WH key=%s", key);
                continue;
            }
            if (atlasW > 0 && atlasH > 0) {
                if (fr.rect.x < 0 || fr.rect.y < 0 ||
                    fr.rect.x + fr.rect.w > atlasW || fr.rect.y + fr.rect.h > atlasH) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "[ANIM] OOB key=%s rect=%d,%d,%d,%d atlas=%d,%d",
                        key, fr.rect.x, fr.rect.y, fr.rect.w, fr.rect.h, atlasW, atlasH);
                    continue;
                }
            }

            if (s_bgFrameCount < (int)SDL_arraysize(s_bgFrames))
                s_bgFrames[s_bgFrameCount++] = fr;
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] frame cap exceeded");
                break;
            }
        }
    }
    // frames as array
    else if (framesArr) {
        size_t count = json_array_get_count(framesArr);
        SDL_Log("[ANIM] frames=array, count=%zu", count);
        for (size_t idx = 0; idx < count; ++idx) {
            JSON_Object* fobj = json_array_get_object(framesArr, idx);
            if (!fobj) continue;

            JSON_Object* rect = json_object_get_object(fobj, "frame");
            if (!rect) { SDL_Log("[ANIM] missing frame at %zu", idx); continue; }

            AnimFrame fr = { 0 };
            fr.rect.x = (int)json_object_get_number(rect, "x");
            fr.rect.y = (int)json_object_get_number(rect, "y");
            fr.rect.w = (int)json_object_get_number(rect, "w");
            fr.rect.h = (int)json_object_get_number(rect, "h");
            fr.duration_ms = (int)json_object_get_number(fobj, "duration");
            if (fr.duration_ms <= 0) fr.duration_ms = 100;

            if (fr.rect.w <= 0 || fr.rect.h <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] invalid WH idx=%zu", idx);
                continue;
            }
            if (atlasW > 0 && atlasH > 0) {
                if (fr.rect.x < 0 || fr.rect.y < 0 ||
                    fr.rect.x + fr.rect.w > atlasW || fr.rect.y + fr.rect.h > atlasH) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "[ANIM] OOB idx=%zu rect=%d,%d,%d,%d atlas=%d,%d",
                        idx, fr.rect.x, fr.rect.y, fr.rect.w, fr.rect.h, atlasW, atlasH);
                    continue;
                }
            }

            if (s_bgFrameCount < (int)SDL_arraysize(s_bgFrames))
                s_bgFrames[s_bgFrameCount++] = fr;
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] frame cap exceeded");
                break;
            }
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] no frames object/array");
    }

    json_value_free(root);

    if (s_bgFrameCount <= 0) {
        SDL_Log("[ANIM] parsed but no valid frames");
        return;
    }

    // 아틀라스 텍스처가 있다면 아틀라스 모드 사용(성능 좋음)
    if (s_bgAtlas) { s_bgFramesUseAtlas = true; return; }

    // (옵션) 아틀라스가 없다면 분할 텍스쳐 생성
    SDL_Surface* atlasSurface = IMG_Load(ASSETS_IMAGES_DIR "garagame-Sheet.png");
    if (!atlasSurface) { SDL_Log("[ANIM] fallback surface load fail: %s", IMG_GetError()); s_bgFrameCount = 0; return; }

    if (atlasSurface->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(atlasSurface, SDL_PIXELFORMAT_RGBA32, 0);
        if (!converted) {
            SDL_Log("[ANIM] surface convert fail: %s", SDL_GetError());
            SDL_FreeSurface(atlasSurface);
            s_bgFrameCount = 0;
            return;
        }
        SDL_FreeSurface(atlasSurface);
        atlasSurface = converted;
    }

    for (int i = 0; i < s_bgFrameCount; ++i) {
        AnimFrame* fr = &s_bgFrames[i];
        SDL_Surface* frameSurface =
            SDL_CreateRGBSurfaceWithFormat(0, fr->rect.w, fr->rect.h, 32, atlasSurface->format->format);
        if (!frameSurface) { SDL_Log("[ANIM] create frame surface fail: %s", SDL_GetError()); continue; }

        if (SDL_BlitSurface(atlasSurface, &fr->rect, frameSurface, NULL) < 0) {
            SDL_Log("[ANIM] blit fail: %s", SDL_GetError());
            SDL_FreeSurface(frameSurface);
            continue;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(G_Renderer, frameSurface);
        SDL_FreeSurface(frameSurface);
        if (!tex) { SDL_Log("[ANIM] create texture fail: %s", SDL_GetError()); continue; }
        fr->texture = tex;
    }

    SDL_FreeSurface(atlasSurface);

    // 유효 텍스쳐만 남기기(compact)
    int write = 0;
    for (int read = 0; read < s_bgFrameCount; ++read) {
        if (!s_bgFrames[read].texture) continue;
        if (write != read) s_bgFrames[write] = s_bgFrames[read];
        ++write;
    }
    s_bgFrameCount = write;

    if (s_bgFrameCount == 0) {
        SDL_Log("[ANIM] no usable splitted textures");
        return;
    }
    SDL_Log("[ANIM] using fallback splitted textures");
}

// -----------------------------
// Scene 콜백
// -----------------------------
static void init(void* arg)
{
    int idx = -1;
    if (arg) idx = (int)(intptr_t)arg;
    else if (G_SelectedPlantIndex >= 0) idx = G_SelectedPlantIndex;

    if (idx < 0) { scene_switch(SCENE_SELECT_PLANT); return; }

    G_SelectedPlantIndex = idx;
    s_plant = plantdb_get(idx);
    if (!s_plant) { scene_switch(SCENE_SELECT_PLANT); return; }

    // 배경 애니메이션
    if (s_bgFrameCount <= 0) {
        load_background_animation();
    }

    SDL_Log("[GAME] bg frames loaded: count=%d, useAtlas=%d", s_bgFrameCount, (int)s_bgFramesUseAtlas);

    s_waterCount = 0;
    s_windowOpen = false;

    if (!s_bgTexture) {
        s_bgTexture = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
        if (!s_bgTexture) SDL_Log("GAMEPLAY: load select_Background.png fail: %s", IMG_GetError());
    }
    if (!s_backIcon) {
        s_backIcon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_exit.png");
        if (!s_backIcon) SDL_Log("GAMEPLAY: load I_exit.png fail: %s", IMG_GetError());
    }

    // 메뉴 BGM이 재생 중이면 부드럽게 끄기
    if (Mix_PlayingMusic()) {
        Mix_FadeOutMusic(200);
        // 굳이 SDL_Delay로 기다릴 필요는 없음. 바로 새 곡 페이드인 시작해도 SDL_mixer가 내부에서 교체 처리.
    }

    // 인게임 BGM: 로드 & 콜백 등록 & 첫 곡 시작
    gameplay_ensure_bgm_loaded();
    Mix_HookMusicFinished(gameplay_on_music_finished);

    int bgm_idx = pick_random_index();
    s_lastBgm = bgm_idx;
    if (s_gameBGMs[bgm_idx]) {
        if (Mix_FadeInMusic(s_gameBGMs[bgm_idx], /*loops=*/1, /*ms=*/200) < 0) {
            SDL_Log("[GAMEPLAY BGM] start error: %s", Mix_GetError());
        }
    }

    // 오디오 볼륨/뮤트 적용
    settings_apply_audio();

    layout();
}

static void handle(SDL_Event* e)
{
    if (e->type == SDL_QUIT) { G_Running = 0; return; }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        layout();
    }

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) { scene_switch_fade(SCENE_MAINMENU, 0.2f, 0.4f); return; }
        if (e->key.keysym.sym == SDLK_w) { on_window(NULL); }
        if (e->key.keysym.sym == SDLK_SPACE) { on_water(NULL); }
    }

    ui_button_handle(&s_btnBack, e);
    ui_button_handle(&s_btnWater, e);
    ui_button_handle(&s_btnWindow, e);
}

static void update(float dt)
{
    if (s_bgFrameCount <= 0) return;

    s_bgFrameElapsedMs += dt * 1000.f;

    int duration = s_bgFrames[s_bgFrameIndex].duration_ms;
    if (duration <= 0) duration = 100;

    while (s_bgFrameElapsedMs >= duration && s_bgFrameCount > 0) {
        s_bgFrameElapsedMs -= duration;
        s_bgFrameIndex = (s_bgFrameIndex + 1) % s_bgFrameCount;

        duration = s_bgFrames[s_bgFrameIndex].duration_ms;
        if (duration <= 0) duration = 100;
    }
}

static void render(SDL_Renderer* r)
{
    int w, h; SDL_GetRendererOutputSize(r, &w, &h);

    // 배경
    bool usingAnimatedBg = false;
    if (s_bgFrameCount > 0) {
        if (s_bgFramesUseAtlas && s_bgAtlas) usingAnimatedBg = true;
        else if (!s_bgFramesUseAtlas && s_bgFrames[s_bgFrameIndex].texture) usingAnimatedBg = true;
    }

    if (usingAnimatedBg) {
        SDL_Rect dst = { 0,0,w,h };
        if (s_bgFramesUseAtlas) {
            SDL_Rect src = s_bgFrames[s_bgFrameIndex].rect;
            SDL_RenderCopy(r, s_bgAtlas, &src, &dst);
        }
        else {
            SDL_Texture* frameTex = s_bgFrames[s_bgFrameIndex].texture;
            if (frameTex) SDL_RenderCopy(r, frameTex, NULL, &dst);
        }
    }
    else if (s_bgAtlas) {
        SDL_Rect dst = { 0,0,w,h };
        SDL_RenderCopy(r, s_bgAtlas, NULL, &dst);
    }
    else {
        // 그라디언트 배경
        for (int y = 0; y < h; ++y) {
            float t = (float)y / (float)h;
            Uint8 R = (Uint8)(16 + t * 20);
            Uint8 G = (Uint8)(20 + t * 60);
            Uint8 B = (Uint8)(28 + t * 40);
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }

    // (옵션) HUD 텍스트
    /*
    if (s_plant) {
        SDL_Color title = {240,236,228,255};
        SDL_Color body  = {210,200,186,255};
        int x=140, y=140;
        draw_text(r, title, x, y, "%s", s_plant->name_kr);
        draw_text(r, body,  x, y+36, "물 준 횟수: %d회 · 권장 %d일", s_waterCount, s_plant->water_days);
        draw_text(r, body,  x, y+72, "창문: %s", s_windowOpen ? "열림" : "닫힘");
    }
    */

    // 버튼 (배경아이콘을 버튼 배경으로 쓰려면 마지막 인자에 s_backIcon 넘기기)
    // ui_button_render(r, G_FontMain, &s_btnBack,   s_backIcon);
    // ui_button_render(r, G_FontMain, &s_btnWater,  NULL);
    // ui_button_render(r, G_FontMain, &s_btnWindow, NULL);
}

static void cleanup(void)
{
    // 애니 프레임 정리
    destroy_frame_textures();
    if (s_bgAtlas) { SDL_DestroyTexture(s_bgAtlas);   s_bgAtlas = NULL; }
    if (s_bgTexture) { SDL_DestroyTexture(s_bgTexture); s_bgTexture = NULL; }
    if (s_backIcon) { SDL_DestroyTexture(s_backIcon);  s_backIcon = NULL; }

    // 인게임 BGM 콜백 해제
    Mix_HookMusicFinished(NULL);

    // 인게임에서만 음악 리소스를 사용한다면 여기서 해제
    for (int i = 0; i < (int)SDL_arraysize(kGameBGMs); ++i) {
        if (s_gameBGMs[i]) { Mix_FreeMusic(s_gameBGMs[i]); s_gameBGMs[i] = NULL; }
    }
    s_bgmLoaded = 0;
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Gameplay" };
Scene* scene_gameplay_object(void) { return &SCENE_OBJ; }
