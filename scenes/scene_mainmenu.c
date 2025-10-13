#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"

static UIButton btns[6];
static const char* labels[6] = { "시작","이어하기","도감","설정","크레딧","종료" };

static SDL_Texture* s_bg = NULL;
static TTF_Font* s_titleFont = NULL;


static void layout(void) {
    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    int bw = (int)(w * 0.33f), bh = 56, gap = 18;
    int total = 5 * bh + 4 * gap;
    int x = (w - bw) / 2, y = (h - total) / 2 + h / 20;
    for (int i = 0;i < 6;i++) {
        btns[i].r = (SDL_Rect){ x, y + i * (bh + gap), bw, bh };
        btns[i].text = labels[i];
        btns[i].enabled = 1; btns[i].hovered = 0;
    }


}

static void init(void) { 
    layout(); 
    int want = IMG_INIT_PNG;
    if ((IMG_Init(want) & want) != want) {
        SDL_Log("IMG_Init: %s", IMG_GetError());
    }
    s_bg = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "bg_title.png");
    if (!s_bg) {
        SDL_Log("Load bg_title.png failed: %s", IMG_GetError());
    }

    s_titleFont = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 48);
    if (!s_titleFont) {
        SDL_Log("TitleFont Open: %s", TTF_GetError());
    }

}
static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { G_Running = 0; return; }
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) layout();

    if (e->type == SDL_MOUSEMOTION) {
        int mx = e->motion.x, my = e->motion.y;
        for (int i = 0;i < 6;i++) btns[i].hovered = ui_point_in_rect(mx, my, &btns[i].r);
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        for (int i = 0;i < 6;i++) if (ui_point_in_rect(mx, my, &btns[i].r)) {
            switch (i) {
            case 0: scene_switch(SCENE_GAMEPLAY); break;
            case 1: scene_switch(SCENE_GAMEPLAY); break;
            case 2: scene_switch(SCENE_CODEX);    break;
            case 3: scene_switch(SCENE_SETTINGS); break;
            case 4: scene_switch_fade(SCENE_CREDITS,0.35f,0.35f); break;
            case 5: G_Running = 0; return;
            }
        }
    }
}
static void update(float dt) { (void)dt; }
static void render(SDL_Renderer* r) {
    if (s_bg) {
        // 배경 이미지를 전체 화면에 꽉 채우기
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        SDL_Rect dst = { 0,0,w,h };
        SDL_RenderCopy(r, s_bg, NULL, &dst);
    }
    else {
        // 이미지가 없으면 그라데이션 배경
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        for (int y = 0; y < h; ++y) {
            float t = (float)y / (float)h;
            Uint8 R = (Uint8)(16 + t * 20);
            Uint8 G = (Uint8)(20 + t * 60);
            Uint8 B = (Uint8)(28 + t * 40);
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }

    // (B) 타이틀 텍스트 (그림자 + 본문 2번 렌더로 간단한 입체감)
    const char* title = "GROWING";
    TTF_Font* titleFont = s_titleFont ? s_titleFont : G_FontMain;
    if (titleFont) {
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        SDL_Color shadow = { 0, 0, 0, 160 };
        SDL_Color mainC = { 235, 245, 235, 255 };

        SDL_Surface* s1 = TTF_RenderUTF8_Blended(titleFont, title, shadow);
        SDL_Surface* s2 = TTF_RenderUTF8_Blended(titleFont, title, mainC);
        if (s1 && s2) {
            SDL_Texture* t1 = SDL_CreateTextureFromSurface(r, s1);
            SDL_Texture* t2 = SDL_CreateTextureFromSurface(r, s2);
            int tw, th; SDL_QueryTexture(t2, NULL, NULL, &tw, &th);

            SDL_Rect d1 = { (w - tw) / 2 + 2, 40 + 2, tw, th }; // 그림자 약간 아래/오른쪽
            SDL_Rect d2 = { (w - tw) / 2,     40,     tw, th }; // 본문

            SDL_RenderCopy(r, t1, NULL, &d1);
            SDL_RenderCopy(r, t2, NULL, &d2);

            SDL_DestroyTexture(t1);
            SDL_DestroyTexture(t2);
        }
        if (s1) SDL_FreeSurface(s1);
        if (s2) SDL_FreeSurface(s2);
    }
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255); SDL_RenderClear(r);
    for (int i = 0;i < 6;i++) ui_draw_button(r, G_FontMain, &btns[i]);
    SDL_RenderPresent(r);
}
static void cleanup(void) {
    if (s_bg) { SDL_DestroyTexture(s_bg); s_bg = NULL; }
    if (s_titleFont) { TTF_CloseFont(s_titleFont); s_titleFont = NULL; }
    }

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "MainMenu" };
Scene* scene_mainmenu_object(void) { return &SCENE_OBJ; }
