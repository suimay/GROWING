#include "scene.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

// 여기에 메인메뉴에서 쓰는 전용 상태/리소스들 선언
// (예: 버튼 배열, 폰트 핸들 등)
typedef struct {
    SDL_Rect rc;
    const char* label;
    int enabled;
    int hovered;
    int focused;      // 키보드 포커스
    float glow;       // 0~1, 호버/포커스 하이라이트 값
} MenuBtn;

enum { BTN_START, BTN_CONTINUE, BTN_COLLECTION, BTN_SETTINGS, BTN_EXIT, BTN_COUNT };
static MenuBtn g_btn[BTN_COUNT];
static int g_focus = 0; // 키보드 포커스 인덱스

// 사운드/폰트/텍스처
static TTF_Font* g_font = NULL;
static Mix_Chunk* sfx_move = NULL;  // 포커스 이동
static Mix_Chunk* sfx_click = NULL; // 클릭
static SDL_Texture* tex_bg = NULL;  // 선택: 배경 이미지 (없어도 OK)


static void layout_mainmenu(int ww, int wh) {
    const int btnW = (int)(ww * 0.33f);
    const int btnH = 56;
    const int gap = 18;
    int totalH = BTN_COUNT * btnH + (BTN_COUNT - 1) * gap;
    int baseX = (ww - btnW) / 2;
    int baseY = (wh - totalH) / 2 + (wh / 20);

    const char* labels[BTN_COUNT] = { "시작","이어하기","컬렉션","설정","종료하기" };
    for (int i = 0;i < BTN_COUNT;i++) {
        g_btn[i].rc = (SDL_Rect){ baseX, baseY + i * (btnH + gap), btnW, btnH };
        g_btn[i].label = labels[i];
        g_btn[i].enabled = 1;
        g_btn[i].hovered = 0;
        g_btn[i].focused = (i == g_focus);
        g_btn[i].glow = 0.f;
    }
}

static void mainmenu_init(void) {
    if (!g_font) {
        TTF_Init();
        g_font = TTF_OpenFont("assets/NotoSansKR.ttf", 28);
        if (!g_font) printf("TTF_OpenFont: %s\n", TTF_GetError());
    }
    //if (!sfx_move)  sfx_move = Mix_LoadWAV("assets/move.wav");
    if (!sfx_click) sfx_click = Mix_LoadWAV("assets/click.wav");

    // 선택: 배경 이미지
    // tex_bg = IMG_LoadTexture(g_ren, "assets/bg_title.png");

    int w, h; SDL_GetRendererOutputSize(g_ren, &w, &h);
    layout_mainmenu(w, h);
}

static void mainmenu_cleanup(void) {
    if (tex_bg) { SDL_DestroyTexture(tex_bg); tex_bg = NULL; }
    if (sfx_move) { Mix_FreeChunk(sfx_move); sfx_move = NULL; }
    if (sfx_click) { Mix_FreeChunk(sfx_click); sfx_click = NULL; }
    if (g_font) { TTF_CloseFont(g_font); g_font = NULL; TTF_Quit(); }
}

static void activate_button(int idx) {
    if (!g_btn[idx].enabled) return;
    Mix_PlayChannel(-1, sfx_click, 0);
    switch (idx) {
    case BTN_START:     switch_scene(SCENE_GAME); break;
    case BTN_CONTINUE:  switch_scene(SCENE_GAME); break;
    case BTN_COLLECTION:switch_scene(SCENE_COLLECTION); break;
    case BTN_SETTINGS:  switch_scene(SCENE_SETTINGS); break;
    case BTN_EXIT:      g_running = 0; break;
    }
}

static void mainmenu_handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { g_running = 0; return; }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        layout_mainmenu(e->window.data1, e->window.data2);
    }

    // 마우스
    if (e->type == SDL_MOUSEMOTION) {
        int mx = e->motion.x, my = e->motion.y;
        for (int i = 0;i < BTN_COUNT;i++) {
            int hov = (mx >= g_btn[i].rc.x && my >= g_btn[i].rc.y &&
                mx < g_btn[i].rc.x + g_btn[i].rc.w && my < g_btn[i].rc.y + g_btn[i].rc.h);
            if (hov && !g_btn[i].hovered) Mix_PlayChannel(-1, sfx_move, 0);
            g_btn[i].hovered = hov;
            if (hov) g_focus = i; // 마우스 올리면 포커스도 이동
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        for (int i = 0;i < BTN_COUNT;i++)
            if (g_btn[i].enabled && SDL_PointInRect(&(SDL_Point) { mx, my }, & g_btn[i].rc))
                activate_button(i);
    }

    // 키보드/패드
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if (k == SDLK_UP || k == SDLK_w) {
            g_focus = (g_focus - 1 + BTN_COUNT) % BTN_COUNT; Mix_PlayChannel(-1, sfx_move, 0);
        }
        else if (k == SDLK_DOWN || k == SDLK_s) {
            g_focus = (g_focus + 1) % BTN_COUNT; Mix_PlayChannel(-1, sfx_move, 0);
        }
        else if (k == SDLK_RETURN || k == SDLK_SPACE) {
            activate_button(g_focus);
        }
        else if (k == SDLK_ESCAPE) {
            g_running = 0;
        }
        for (int i = 0;i < BTN_COUNT;i++) g_btn[i].focused = (i == g_focus);
    }
}

static void draw_button(SDL_Renderer* r, MenuBtn* b, float dt) {
    // glow 값 보간
    float target = (b->hovered || b->focused) ? 1.f : 0.f;
    float speed = 8.f; // 반응 속도
    b->glow += (target - b->glow) * SDL_min(1.f, dt * speed);

    SDL_Color base = b->enabled ? (SDL_Color) { 50, 90, 170, 255 } : (SDL_Color) { 90, 90, 90, 255 };
    // glow에 따라 밝기 상승
    int add = (int)(b->glow * 70);
    SDL_SetRenderDrawColor(r, base.r + add, base.g + add, base.b + add, 255);
    SDL_RenderFillRect(r, &b->rc);

    // 테두리
    SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)(120 + b->glow * 100));
    SDL_RenderDrawRect(r, &b->rc);

    // 라벨
    if (g_font && b->label) {
        SDL_Color fg = { 230,230,230,255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(g_font, b->label, fg);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            int w, h; SDL_QueryTexture(t, NULL, NULL, &w, &h);
            SDL_Rect d = { b->rc.x + (b->rc.w - w) / 2, b->rc.y + (b->rc.h - h) / 2, w, h };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
}

static float g_fade = 0.f; // 0=보임, 1=검정
static int   g_fade_out = 0;

static void mainmenu_update(float dt) {
    // 씬 전환 시 사용하려면 외부에서 g_fade_out=1 하고, g_fade를 1로 보간하여 다음 씬으로.
    if (g_fade_out) { g_fade += dt * 1.5f; if (g_fade > 1.f) g_fade = 1.f; }
    else { g_fade -= dt * 1.5f; if (g_fade < 0.f) g_fade = 0.f; }
}

static void mainmenu_render(SDL_Renderer* r) {
    // 배경
    if (tex_bg) {
        SDL_RenderCopy(r, tex_bg, NULL, NULL);
    }
    else {
        // 간단 그라데이션
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        for (int y = 0;y < h;y++) {
            float t = (float)y / (float)h;
            Uint8 R = (Uint8)(16 + t * 20);
            Uint8 G = (Uint8)(20 + t * 60);
            Uint8 B = (Uint8)(28 + t * 40);
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }

    // 타이틀 띠
    SDL_Rect bar = { 0, 90, 1920, 4 }; // 가로는 실제 w로 바꿔도 됨
    SDL_SetRenderDrawColor(r, 80, 160, 120, 255);
    SDL_RenderFillRect(r, &bar);

    // 타이틀 텍스트
    if (g_font) {
        SDL_Color c = { 240,250,240,255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(g_font, "GROWING", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            int w, h; SDL_GetRendererOutputSize(r, &w, &h);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { (w - tw) / 2, 40, tw, th };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    // 버튼들
    Uint64 now = SDL_GetPerformanceCounter();
    static Uint64 last = 0; if (!last) last = now;
    float dt = (float)((now - last) / (double)SDL_GetPerformanceFrequency());
    last = now;

    for (int i = 0;i < BTN_COUNT;i++) draw_button(r, &g_btn[i], dt);

    // 페이드 오버레이
    if (g_fade > 0.f) {
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(g_fade * 255));
        SDL_Rect full = { 0,0,w,h }; SDL_RenderFillRect(r, &full);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    SDL_RenderPresent(r);
}




/* ❗❗ 여기서는 'static'을 쓰면 안 됩니다. 전역으로 정의해야 링크 가능 */
Scene MAIN_MENU = {
    mainmenu_init,
    mainmenu_handle,
    mainmenu_update,
    mainmenu_render,
    mainmenu_cleanup
};

