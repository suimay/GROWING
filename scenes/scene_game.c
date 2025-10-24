// scenes/scene_gameplay.c
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"   // PlantInfo, plantdb_get
#include "../include/save.h"
#include <stdio.h>
#include <stdbool.h>

static const PlantInfo* s_plant = NULL;
static UIButton s_btnBack;        // 메인으로
static UIButton s_btnWater;       // (샘플) 물 주기
static UIButton s_btnWindow;      // 창문 열기
static int s_waterCount = 0;      // 시각적 확인용
static bool s_windowoc = false;
static SDL_Texture* s_bgTexture = NULL;
static SDL_Texture* s_backIcon = NULL;
static SDL_Texture* s_plantIcon = NULL;
static SDL_Texture* make_text(SDL_Renderer* r, TTF_Font* f, const char* s, SDL_Color c) {
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s, c);
    if (!surf) return NULL;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}
// ---- 콜백 ----
static void on_water(void* ud) {
    (void)ud;
    s_waterCount++;
    // 예: 120ml 고정 테스트
    if (s_plant) log_water(s_plant->id, 120);
}



// 성장 단계 전환(예: 일정 경험치/일수 도달 시)
static void go_next_stage(const char* nextStageName) {
    if (s_plant) {
        log_stage(s_plant->id, nextStageName);
        // 최종 단계라면 도감 해금
        if (SDL_strcmp(nextStageName, "Final") == 0) {
            save_mark_plant_completed(s_plant->id);
        }
    }
}

// 햇빛 제공 기록 (너 로직에 맞게 호출)
static void grant_sun_minutes(int minutes, int ppfd) {
    if (s_plant) log_sun(s_plant->id, minutes, ppfd);
}


static void on_back(void* ud) { (void)ud; scene_switch(SCENE_SELECT_PLANT); }

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
static void init(void) {
    // 선택된 식물 참조 (없으면 선택씬으로)
    if (G_SelectedPlantIndex >= 0)
        s_plant = plantdb_get(G_SelectedPlantIndex);
    else
        s_plant = NULL;

    if (!s_plant) { scene_switch(SCENE_SELECT_PLANT); return; }

    s_waterCount = 0;
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
    if (s_plantIcon) {
        SDL_DestroyTexture(s_plantIcon);
        s_plantIcon = NULL;
    }
    if (s_plant && s_plant->icon_path[0]) {
        s_plantIcon = IMG_LoadTexture(G_Renderer, s_plant->icon_path);
        if (!s_plantIcon) {
            SDL_Log("GAMEPLAY: failed to load plant icon %s: %s", s_plant->icon_path, IMG_GetError());
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

    const PlantInfo* p = s_plant;
    SDL_Rect board = { 140, 140, w - 280, h - 260 };
    if (board.w < 360) board.w = 360;
    if (board.h < 260) board.h = 260;

    SDL_SetRenderDrawColor(r, 179, 139, 98, 255);
    SDL_RenderFillRect(r, &board);
    SDL_SetRenderDrawColor(r, 92, 63, 44, 255);
    SDL_RenderDrawRect(r, &board);

    SDL_Rect inner = { board.x + 24, board.y + 24, board.w - 48, board.h - 48 };
    SDL_SetRenderDrawColor(r, 236, 216, 188, 255);
    SDL_RenderFillRect(r, &inner);
    SDL_SetRenderDrawColor(r, 141, 101, 66, 160);
    SDL_RenderDrawRect(r, &inner);

    if (G_FontMain && p) {
        SDL_Color nameColor = { 64, 40, 22, 255 };
        SDL_Color labelColor = { 92, 63, 44, 255 };
        SDL_Color subtleColor = { 128, 96, 70, 255 };

        int contentPadding = 28;
        int iconSlot = 200;
        int textX = inner.x + contentPadding;
        int cursorY = inner.y + contentPadding;

        if (s_plantIcon) {
            int texW = 0, texH = 0;
            SDL_QueryTexture(s_plantIcon, NULL, NULL, &texW, &texH);
            if (texW > 0 && texH > 0) {
                float scale = (float)iconSlot / (float)((texW > texH) ? texW : texH);
                if (scale > 1.f) scale = 1.f;
                int drawW = (int)(texW * scale);
                int drawH = (int)(texH * scale);
                SDL_Rect iconArea = { textX, cursorY, iconSlot, iconSlot };
                SDL_Rect iconDst = {
                    iconArea.x + (iconArea.w - drawW) / 2,
                    iconArea.y + (iconArea.h - drawH) / 2,
                    drawW,
                    drawH};
                SDL_RenderCopy(r, s_plantIcon, NULL, &iconDst);
            }
        }
        if (s_plantIcon)
            textX += iconSlot + 32;

        int lineY = cursorY;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(G_FontMain, p->name_kr, nameColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int tw = surf->w;
            int th = surf->h;
            int scaledW = (int)(tw * 1.6f + 0.5f);
            int scaledH = (int)(th * 1.6f + 0.5f);
            SDL_Rect dst = { textX, lineY, scaledW, scaledH };
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
            lineY += scaledH + 16;
        }

        char latinLine[128];
        SDL_snprintf(latinLine, sizeof(latinLine), "학명: %s", p->latin);
        SDL_Surface* latinSurf = TTF_RenderUTF8_Blended(G_FontMain, latinLine, subtleColor);
        if (latinSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, latinSurf);
            int tw = latinSurf->w;
            int th = latinSurf->h;
            SDL_Rect dst = { textX, lineY, tw, th };
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(latinSurf);
            lineY += th + 28;
        }

        int lightIdx = p->light_level;
        if (lightIdx < 0) lightIdx = 0;
        if (lightIdx > 2) lightIdx = 2;
        const char* lightName[] = { "약", "중간", "강" };
        const char* lightDesc[] = { "은은한 간접광", "밝은 간접광", "직사광선" };

        char waterLine[96];
        SDL_snprintf(waterLine, sizeof(waterLine), "물 주기: %d일 간격 (현재 %d회)", p->water_days, s_waterCount);
        char lightLine[96];
        SDL_snprintf(lightLine, sizeof(lightLine), "햇빛: %s - %s", lightName[lightIdx], lightDesc[lightIdx]);
        char tempLine[96];
        SDL_snprintf(tempLine, sizeof(tempLine), "적정 온도: %d℃ ~ %d℃", p->min_temp, p->max_temp);
        char windowLine[64];
        SDL_snprintf(windowLine, sizeof(windowLine), "창문 상태: %s", s_windowoc ? "열려있음" : "닫혀있음");

        const char* lines[] = { waterLine, lightLine, tempLine, windowLine };
        for (size_t i = 0; i < SDL_arraysize(lines); ++i) {
            SDL_Surface* lineSurf = TTF_RenderUTF8_Blended(G_FontMain, lines[i], labelColor);
            if (!lineSurf) continue;
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, lineSurf);
            SDL_Rect dst = { textX, lineY, lineSurf->w, lineSurf->h };
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            lineY += lineSurf->h + 20;
            SDL_FreeSurface(lineSurf);
        }

        lineY += 8;
        char tipWater[128];
        SDL_snprintf(tipWater, sizeof(tipWater), "Tip: 흙이 마르면 %d일 이내에 충분히 물을 주세요.", p->water_days);
        char tipLight[128];
        SDL_snprintf(tipLight, sizeof(tipLight), "Tip: %s 환경을 유지하면 건강하게 자라요.", lightDesc[lightIdx]);
        const char* tips[] = { tipWater, tipLight };
        for (size_t i = 0; i < SDL_arraysize(tips); ++i) {
            SDL_Surface* tipSurf = TTF_RenderUTF8_Blended(G_FontMain, tips[i], subtleColor);
            if (!tipSurf) continue;
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, tipSurf);
            SDL_Rect dst = { textX, lineY, tipSurf->w, tipSurf->h };
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            lineY += tipSurf->h + 12;
            SDL_FreeSurface(tipSurf);
        }
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
    if (s_plantIcon) {
        SDL_DestroyTexture(s_plantIcon);
        s_plantIcon = NULL;
    }
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Gameplay" };
Scene* scene_gameplay_object(void) { return &SCENE_OBJ; }
