// scenes/scene_gameplay.c
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"   // PlantInfo, plantdb_get
#include <stdio.h>
#include <stdbool.h>

static const PlantInfo* s_plant = NULL;
static UIButton s_btnBack;        // 메인으로
static UIButton s_btnWater;       // (샘플) 물 주기
static UIButton s_btnWindow;      // 창문 열기
static int s_waterCount = 0;      // 시각적 확인용
static bool s_windowoc = false;        
static SDL_Texture* make_text(SDL_Renderer* r, TTF_Font* f, const char* s, SDL_Color c) {
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s, c);
    if (!surf) return NULL;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}
// ---- 콜백 ----
static void on_back(void* ud) { (void)ud; scene_switch(SCENE_MAINMENU); }
static void on_water(void* ud) { (void)ud; s_waterCount++; SDL_Log("[GAME] water++ -> %d", s_waterCount); }
static void on_window(void* ud) {
    (void)ud; 
    s_windowoc = !s_windowoc;
    ui_button_init(&s_btnWindow, s_btnWindow.r,
        s_windowoc ? "창문 닫기" : "창문 열기");
    ui_button_set_callback(&s_btnWindow, on_window, NULL);  
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);      
    SDL_Log(s_windowoc ? "[GAME] window open." : "[GAME] window close.");
            
}

// ---- 레이아웃 ----
static void layout(void) {
    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    ui_button_init(&s_btnBack, (SDL_Rect) { 20, 20, 120, 44 }, "← 뒤로");
    ui_button_init(&s_btnWater, (SDL_Rect) { 20, h - 64 - 20, 160, 64 }, "물 주기");
    ui_button_init(&s_btnWindow, (SDL_Rect) { 200, h - 64 - 20, 160, 64 }, "창문 여닫기");

    ui_button_set_callback(&s_btnBack, on_back, NULL);
    ui_button_set_callback(&s_btnWater, on_water, NULL);
    ui_button_set_callback(&s_btnWindow, on_window, NULL);

    // 효과음 전역 사용 중이면 이렇게:
    ui_button_set_sfx(&s_btnBack, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWater, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);
}

// ---- Scene 콜백 ----
static void init(void) {
    // 선택된 식물 참조 (없으면 선택씬으로)
    if (G_SelectedPlantIndex >= 0)
        s_plant = plantdb_get(G_SelectedPlantIndex);
    else
        s_plant = NULL;

    if (!s_plant) { scene_switch(SCENE_SELECT_PLANT); return; }

    s_waterCount = 0;
    layout();
}

static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { G_Running = 0; return; }
    
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        layout();
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) { scene_switch(SCENE_MAINMENU); return; }
        if (e->key.keysym.sym == SDLK_w) { on_window(NULL); }
        if (e->key.keysym.sym == SDLK_SPACE) { on_water(NULL); }
    }
        

    ui_button_handle(&s_btnBack, e);
    ui_button_handle(&s_btnWater, e);
    ui_button_handle(&s_btnWindow, e);
}

static void update(float dt) { (void)dt; /* 최소 버전: 로직 없음 */ }

static void render(SDL_Renderer* r) {
    // 배경 판넬
    int w, h; SDL_GetRendererOutputSize(r, &w, &h);
    SDL_Rect panel = { 180, 80, w - 220, h - 160 };
    SDL_SetRenderDrawColor(r, 30, 42, 62, 255); SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 200, 210, 230, 140); SDL_RenderDrawRect(r, &panel);
    // 상단 타이틀/정보
    if (G_FontMain) {
        SDL_Color c1 = { 235,245,240,255 };
        SDL_Color c2 = { 200,210,220,255 };
        SDL_Color c3 = { 200,210,220,255 };
        char line1[128], line2[128], line3[128];

        SDL_snprintf(line1, sizeof(line1), "키우는 식물: %s", s_plant ? s_plant->name_kr : "(없음)");
        SDL_snprintf(line2, sizeof(line2), "물 준 횟수: %d회  ·  권장 주기: %d일", s_waterCount,
            s_plant ? s_plant->water_days : 0);

        SDL_snprintf(line3, sizeof(line3), "창문 상태: %s", s_windowoc ? "열려있음" : "닫혀있음");

        

        SDL_Surface* s1 = TTF_RenderUTF8_Blended(G_FontMain, line1, c1);
        SDL_Surface* s2 = TTF_RenderUTF8_Blended(G_FontMain, line2, c2);
        SDL_Surface* s3 = TTF_RenderUTF8_Blended(G_FontMain, line3, c3);
        if (s1) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s1);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { panel.x + 20, panel.y + 20, tw, th };
            SDL_RenderCopy(r, t, NULL, &d); SDL_DestroyTexture(t); SDL_FreeSurface(s1);
        }
        if (s2) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s2);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { panel.x + 20, panel.y + 60, tw, th };
            SDL_RenderCopy(r, t, NULL, &d); SDL_DestroyTexture(t); SDL_FreeSurface(s2);
        }
        if (s3) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s3);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { panel.x + 20, panel.y + 100, tw, th };
            SDL_RenderCopy(r, t, NULL, &d); SDL_DestroyTexture(t); SDL_FreeSurface(s3);
        }
    }

    // 버튼
    ui_button_render(r, G_FontMain, &s_btnBack);
    ui_button_render(r, G_FontMain, &s_btnWater);
    ui_button_render(r, G_FontMain, &s_btnWindow);

    // (향후) 식물 스프라이트, 게이지 등은 여기 추가
}

static void cleanup(void) {
    // 버튼은 소멸자 필요 없음. 외부 리소스도 없음.
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Gameplay" };
Scene* scene_gameplay_object(void) { return &SCENE_OBJ; }
