// scenes/scene_gameplay.c
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"   // PlantInfo, plantdb_get
#include "../include/save.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

static const PlantInfo* s_plant = NULL;
static UIButton s_btnBack;        // 메인으로
static UIButton s_btnWater;       // (샘플) 물 주기
static UIButton s_btnWindow;      // 창문 열기
static int s_waterCount = 0;      // 시각적 확인용
static bool s_windowoc = false;
static SDL_Texture* s_bgTexture = NULL;
static SDL_Texture* s_backIcon = NULL;

static void draw_text(SDL_Renderer *r, SDL_Color color, int x, int y, const char *fmt, ...)
{
    if (!G_FontMain || !fmt)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_Surface *surf = TTF_RenderUTF8_Blended(G_FontMain, buf, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}
// ---- 콜백 ----
static void on_water(void* ud) {
    (void)ud;
    s_waterCount++;
    // 예: 120ml 고정 테스트
    if (s_plant) log_water(s_plant->id, 120);
}



static void on_back(void* ud)
{
    (void)ud;
    if (s_plant)
        scene_switch_arg(SCENE_PLANTINFO, (void *)(intptr_t)G_SelectedPlantIndex);
    else
        scene_switch(SCENE_SELECT_PLANT);
}

//창문 토글
static void on_window(void* ud) {
    (void)ud; 
    s_windowoc = !s_windowoc;
    ui_button_init(&s_btnWindow, s_btnWindow.r,
        s_windowoc ? "창문 닫기" : "창문 열기");
    ui_button_set_callback(&s_btnWindow, on_window, NULL);  
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);      
    SDL_Log(s_windowoc ? "[GAME] window open." : "[GAME] window close.");
    if (s_plant) log_window(s_plant->id, s_windowoc);
            
}

// ---- 레이아웃 ----
static void layout(void) {
    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    const int margin = 40;
    const int backSize = 72;
    const int actionWidth = 200;
    const int actionHeight = 68;
    const int actionGap = 24;
    ui_button_init(&s_btnBack, (SDL_Rect){margin, margin, backSize, backSize}, "");
    ui_button_init(&s_btnWater, (SDL_Rect){margin, h - actionHeight - margin, actionWidth, actionHeight}, "물 주기");
    ui_button_init(&s_btnWindow, (SDL_Rect){margin + actionWidth + actionGap, h - actionHeight - margin, actionWidth, actionHeight}, "창문 여닫기");

    ui_button_set_callback(&s_btnBack, on_back, NULL);
    ui_button_set_callback(&s_btnWater, on_water, NULL);
    ui_button_set_callback(&s_btnWindow, on_window, NULL);

    // 효과음 전역 사용 중이면 이렇게:
    ui_button_set_sfx(&s_btnBack, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWater, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);
}

// ---- Scene 콜백 ----
static void init(void *arg) {
    int idx = -1;
    if (arg)
        idx = (int)(intptr_t)arg;
    else if (G_SelectedPlantIndex >= 0)
        idx = G_SelectedPlantIndex;

    if (idx < 0) {
        scene_switch(SCENE_SELECT_PLANT);
        return;
    }

    G_SelectedPlantIndex = idx;
    s_plant = plantdb_get(idx);
    if (!s_plant) {
        scene_switch(SCENE_SELECT_PLANT);
        return;
    }

    s_waterCount = 0;
    s_windowoc = false;
    if (!s_bgTexture) {
        s_bgTexture = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
        if (!s_bgTexture) {
            SDL_Log("GAMEPLAY: failed to load select_Background.png: %s", IMG_GetError());
        }
    }
    if (!s_backIcon) {
        s_backIcon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
        if (!s_backIcon) {
            SDL_Log("GAMEPLAY: failed to load I_back.png: %s", IMG_GetError());
        }
    }
    layout();
}

static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { G_Running = 0; return; }
    
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        layout();
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) { scene_switch(SCENE_SELECT_PLANT); return; }
        if (e->key.keysym.sym == SDLK_w) { on_window(NULL); }
        if (e->key.keysym.sym == SDLK_SPACE) { on_water(NULL); }
    }
        

    ui_button_handle(&s_btnBack, e);
    ui_button_handle(&s_btnWater, e);
    ui_button_handle(&s_btnWindow, e);
}

static void update(float dt) { (void)dt; /* 최소 버전: 로직 없음 */ }

static void render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    SDL_RenderClear(r);

    int w, h; SDL_GetRendererOutputSize(r, &w, &h);
    if (s_bgTexture) {
        SDL_Rect dst = { 0, 0, w, h };
        SDL_RenderCopy(r, s_bgTexture, NULL, &dst);
    } else {
        for (int y = 0; y < h; ++y) {
            float t = (float)y / (float)h;
            Uint8 R = (Uint8)(16 + t * 20);
            Uint8 G = (Uint8)(20 + t * 60);
            Uint8 B = (Uint8)(28 + t * 40);
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }

    SDL_Color titleColor = {240, 236, 228, 255};
    SDL_Color bodyColor = {210, 200, 186, 255};
    int hudX = 140;
    int hudY = 140;
    SDL_Rect hud = {hudX - 20, hudY - 20, 480, 180};
    SDL_SetRenderDrawColor(r, 42, 62, 84, 180);
    SDL_RenderFillRect(r, &hud);
    SDL_SetRenderDrawColor(r, 18, 28, 38, 220);
    SDL_RenderDrawRect(r, &hud);

    if (s_plant && G_FontMain)
    {
        draw_text(r, titleColor, hudX, hudY, "%s", s_plant->name_kr);
        draw_text(r, bodyColor, hudX, hudY + 36, "학명: %s", s_plant->latin);
        draw_text(r, bodyColor, hudX, hudY + 72, "물 준 횟수: %d회 (추천 주기 %d일)", s_waterCount, s_plant->water_days);
        const char *lightNames[] = {"약", "중간", "강"};
        int lightIdx = s_plant->light_level;
        if (lightIdx < 0) lightIdx = 0;
        if (lightIdx > 2) lightIdx = 2;
        draw_text(r, bodyColor, hudX, hudY + 108, "빛: %s  |  온도: %d-%d℃", lightNames[lightIdx], s_plant->min_temp, s_plant->max_temp);
        draw_text(r, bodyColor, hudX, hudY + 144, "창문 상태: %s", s_windowoc ? "열려 있음" : "닫혀 있음");
    }

    // 버튼
    ui_button_render(r, G_FontMain, &s_btnBack, s_backIcon);
    ui_button_render(r, G_FontMain, &s_btnWater, NULL);
    ui_button_render(r, G_FontMain, &s_btnWindow, NULL);

    // (향후) 식물 스프라이트, 게이지 등은 여기 추가
}

static void cleanup(void) {
    // 버튼은 소멸자 필요 없음. 외부 리소스도 없음.
    if (s_bgTexture) {
        SDL_DestroyTexture(s_bgTexture);
        s_bgTexture = NULL;
    }
    if (s_backIcon) {
        SDL_DestroyTexture(s_backIcon);
        s_backIcon = NULL;
    }
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Gameplay" };
Scene* scene_gameplay_object(void) { return &SCENE_OBJ; }
