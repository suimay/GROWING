#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"

#define BTN_COUNT 6

static UIButton s_buttons[BTN_COUNT];
static Mix_Chunk* sfx_click = NULL;
static Mix_Chunk* sfx_hover = NULL;

static void on_start(void* ud) { (void)ud; scene_switch(SCENE_SELECT_PLANT); }

static void on_continue(void* ud) { (void)ud; scene_switch(SCENE_GAMEPLAY); }
static void on_codex(void* ud) { (void)ud; scene_switch(SCENE_CODEX); }
static void on_settings(void* ud) { (void)ud; scene_switch(SCENE_SETTINGS); }
static void on_credits(void* ud) { (void)ud; scene_switch_fade(SCENE_CREDITS, 0.2f, 0.4f); }
static void on_exit(void* ud) { (void)ud; G_Running = 0; }
static SDL_Texture* s_bg = NULL;
static TTF_Font* s_titleFont = NULL;


static void layout(void) {
    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    int bw = (int)(w * 0.33f), bh = 56, gap = 18;
    int total = 5 * bh + 4 * gap;
    int x = (w - bw) / 2, y = (h - total) / 2 + h / 20;
    

    const char* labels[BTN_COUNT] = { "시작","이어하기","도감","설정","크레딧","종료" };
    UIButtonOnClick funcs[BTN_COUNT] = { on_start,on_continue,on_codex,on_settings,on_credits,on_exit };

    for (int i = 0;i < BTN_COUNT;i++) {
        ui_button_init(&s_buttons[i], (SDL_Rect) { x, y + i * (bh + gap), bw, bh }, labels[i]);
        ui_button_set_callback(&s_buttons[i], funcs[i], NULL);
        ui_button_set_sfx(&s_buttons[i], sfx_click, sfx_hover);
    }


}

static void init(void) { 
    layout(); 
    int want = IMG_INIT_PNG;
    if ((IMG_Init(want) & want) != want) {
        SDL_Log("IMG_Init: %s", IMG_GetError());
    }
    s_bg = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "bg_title.jpg");
    if (!s_bg) {
        SDL_Log("Load bg_title.png failed: %s", IMG_GetError());
    }

    s_titleFont = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 48);
    if (!s_titleFont) {
        SDL_Log("TitleFont Open: %s", TTF_GetError());
    }

    SDL_Log("[MAINMENU] TTF_WasInit=%d", TTF_WasInit());
    {
        const char* p = ASSETS_FONTS_DIR "NotoSansKR.ttf";
        SDL_RWops* rw = SDL_RWFromFile(p, "rb");
        SDL_Log("[MAINMENU] font path: %s, exist=%s", p, rw ? "YES" : "NO");
        if (rw) SDL_RWclose(rw);
    }

    if (!s_titleFont && !G_FontMain) {
        SDL_Log("[MAINMENU] No font available (both title and global NULL)");
    }

    // render() 타이틀 그리기 직전에
    SDL_Log("[MAINMENU] render title try");

}
static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { G_Running = 0; return; }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) layout();
    for (int i = 0;i < BTN_COUNT;i++) ui_button_handle(&s_buttons[i], e);

    
}
static void update(float dt) { (void)dt; }
static void render(SDL_Renderer* r) {
    // 0) 매 프레임 시작할 때만 Clear
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    

    // 1) 배경
    if (s_bg) {
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        SDL_Rect dst = { 0,0,w,h };
        SDL_RenderCopy(r, s_bg, NULL, &dst);
    }
    else {
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

    // 2) 타이틀
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

            SDL_Rect d1 = { (w - tw) / 2 + 2, 40 + 2, tw, th };
            SDL_Rect d2 = { (w - tw) / 2,     40,     tw, th };

            if (t1) SDL_RenderCopy(r, t1, NULL, &d1);
            if (t2) SDL_RenderCopy(r, t2, NULL, &d2);

            if (t1) SDL_DestroyTexture(t1);
            if (t2) SDL_DestroyTexture(t2);
        }
        if (s1) SDL_FreeSurface(s1);
        if (s2) SDL_FreeSurface(s2);
    }

    // 3) 서브 타이틀
    if (G_FontMain) {
        SDL_Color subC = { 210, 230, 220, 220 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(G_FontMain, "날씨 기반 반려식물 시뮬레이터", subC);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            int w, h; SDL_GetRendererOutputSize(r, &w, &h);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { (w - tw) / 2, 40 + 48 + 12, tw, th };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    // 4) 버튼
    for (int i = 0; i < BTN_COUNT; i++) {
        ui_button_render(r, G_FontMain, &s_buttons[i]);
    }
   
}

static void cleanup(void) {
    if (s_bg) { SDL_DestroyTexture(s_bg); s_bg = NULL; }
    if (s_titleFont) { TTF_CloseFont(s_titleFont); s_titleFont = NULL; }
    }

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "MainMenu" };
Scene* scene_mainmenu_object(void) { return &SCENE_OBJ; }
