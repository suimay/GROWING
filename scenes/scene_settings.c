// scene_settings.c
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"   // Scene 구조체, switch_scene() 선언 포함


static SDL_Texture* s_bg = NULL;
static TTF_Font* s_titleFont = NULL;
// ---------------- 슬라이더 구조 ----------------
typedef struct {
    SDL_Rect bar;
    SDL_Rect handle;
    float value;      // 0.0 ~ 1.0
    int dragging;
} Slider;

// ---------------- 버튼 구조 ----------------
typedef struct {
    SDL_Rect rc;
    const char* label;
} Button;

// ---------------- 전역 ----------------
static Slider bgm_slider;
static Slider sfx_slider;
static Button sfx_button;
static Button btn_test;
static Button btn_back;
static Mix_Chunk* sfx_click = NULL;
static TTF_Font* font = NULL;

// ---------------- 함수 선언 ----------------
static void settings_init(void);
static void settings_handle(SDL_Event* e);
static void settings_update(float dt);
static void settings_render(SDL_Renderer* r);
static void settings_cleanup(void);

// ---------------- 유틸 함수 ----------------
static int point_in_rect(int x, int y, SDL_Rect rc) {
    return (x >= rc.x && y >= rc.y && x < rc.x + rc.w && y < rc.y + rc.h);
}

static void draw_text(SDL_Renderer* r, const char* text, int x, int y) {
    if (!font) return;
    SDL_Color fg = { 230,230,230,255 };
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, fg);
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = { x, y, s->w, s->h };
    SDL_RenderCopy(r, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

static void draw_slider(SDL_Renderer* r, Slider* s) {
    // 바 (트랙)
    SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
    SDL_RenderFillRect(r, &s->bar);

    // 채워진 부분
    SDL_Rect filled = s->bar;
    filled.w = (int)(s->bar.w * s->value);
    SDL_SetRenderDrawColor(r, 90, 180, 120, 255);
    SDL_RenderFillRect(r, &filled);

    // 핸들
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &s->handle);
}

// ---------------- 초기화 ----------------
static void settings_init(void) {
    int cx = 960 / 2;
    int baseY = 200;

    // BGM 슬라이더
    bgm_slider.bar = (SDL_Rect){ cx - 200, baseY, 400, 10 };
    bgm_slider.handle = (SDL_Rect){ cx - 10, baseY - 5, 20, 20 };
    bgm_slider.value = 0.5f;
    bgm_slider.dragging = 0;

    // SFX 슬라이더
    sfx_button.rc = (SDL_Rect){ cx - 200, baseY + 100, 400, 10 };
    sfx_button.label = "효과음 On/Off";
    /*
    sfx_slider.bar = (SDL_Rect){ cx - 200, baseY + 100, 400, 10 };
    sfx_slider.handle = (SDL_Rect){ cx - 10, baseY + 95, 20, 20 };
    sfx_slider.value = 0.5f;
    sfx_slider.dragging = 0;
    */
    
    // 버튼
    btn_test.rc = (SDL_Rect){ cx - 150, baseY + 200, 150, 40 };
    btn_test.label = "효과음 테스트";
    btn_back.rc = (SDL_Rect){ cx + 30, baseY + 200, 120, 40 };
    btn_back.label = "뒤로가기";

    // 폰트 로드
    if (!font) {
        TTF_Init();
        font = TTF_OpenFont("assets/NotoSansKR.ttf", 22);
        if (!font) printf("TTF_OpenFont Error: %s\n", TTF_GetError());
    }

    // 효과음 로드
    if (!sfx_click) {
        sfx_click = Mix_LoadWAV("assets/click.wav");
        if (!sfx_click) printf("SFX Load Error: %s\n", Mix_GetError());
    }
}

// ---------------- 이벤트 ----------------
static void settings_handle(SDL_Event* e) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    switch (e->type) {
    case SDL_QUIT:
        G_Running = 0;
        break;
    case SDL_KEYDOWN:
        if (e->key.keysym.sym == SDLK_ESCAPE)
            scene_switch(SCENE_MAINMENU);
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (e->button.button == SDL_BUTTON_LEFT) {
            // BGM 슬라이더
            if (point_in_rect(mx, my, bgm_slider.handle)) bgm_slider.dragging = 1;
            // SFX on / off
            else if (point_in_rect(mx, my, sfx_button.rc)) {
                Mix_PlayChannel(-1, sfx_click, 0);
            }
            // 효과음 테스트
            else if (point_in_rect(mx, my, btn_test.rc)) {
                Mix_PlayChannel(-1, sfx_click, 0);
            }
            // 뒤로가기
            else if (point_in_rect(mx, my, btn_back.rc)) {
                scene_switch(SCENE_MAINMENU);
            }
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (e->button.button == SDL_BUTTON_LEFT) {
            bgm_slider.dragging = 0;
            sfx_slider.dragging = 0;
        }
        break;
    case SDL_MOUSEMOTION:
        if (bgm_slider.dragging) {
            float pos = (float)(mx - bgm_slider.bar.x) / bgm_slider.bar.w;
            if (pos < 0) pos = 0; if (pos > 1) pos = 1;
            bgm_slider.value = pos;
            bgm_slider.handle.x = bgm_slider.bar.x + (int)(bgm_slider.bar.w * pos) - bgm_slider.handle.w / 2;
            Mix_VolumeMusic((int)(bgm_slider.value * MIX_MAX_VOLUME));
        }
        if (sfx_slider.dragging) {
            float pos = (float)(mx - sfx_slider.bar.x) / sfx_slider.bar.w;
            if (pos < 0) pos = 0; if (pos > 1) pos = 1;
            sfx_slider.value = pos;
            sfx_slider.handle.x = sfx_slider.bar.x + (int)(sfx_slider.bar.w * pos) - sfx_slider.handle.w / 2;
            Mix_VolumeChunk(sfx_click, (int)(sfx_slider.value * MIX_MAX_VOLUME));
        }
        break;
    }
}

// ---------------- 업데이트 ----------------
static void settings_update(float dt) { (void)dt; }

// ---------------- 렌더 ----------------
static void settings_render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    

    draw_text(r, "설정", 450, 100);
    draw_text(r, "BGM 볼륨", bgm_slider.bar.x, bgm_slider.bar.y - 30);
    draw_slider(r, &bgm_slider);

    draw_text(r, "효과음 볼륨", sfx_slider.bar.x, sfx_slider.bar.y - 30);
    draw_slider(r, &sfx_slider);

    // 버튼
    SDL_SetRenderDrawColor(r, 70, 140, 255, 255);
    SDL_RenderFillRect(r, &btn_test.rc);
    SDL_RenderFillRect(r, &btn_back.rc);
    draw_text(r, btn_test.label, btn_test.rc.x + 10, btn_test.rc.y + 10);
    draw_text(r, btn_back.label, btn_back.rc.x + 20, btn_back.rc.y + 10);

    
}

// ---------------- 정리 ----------------
static void settings_cleanup(void) {
    if (sfx_click) {
        Mix_FreeChunk(sfx_click);
        sfx_click = NULL;
    }
    if (font) {
        TTF_CloseFont(font);
        font = NULL;
        TTF_Quit();
    }
}

//
static void init(void) {
    
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

    SDL_Log("[SETTIGNS] TTF_WasInit=%d", TTF_WasInit());
    {
        const char* p = ASSETS_FONTS_DIR "NotoSansKR.ttf";
        SDL_RWops* rw = SDL_RWFromFile(p, "rb");
        SDL_Log("[SETTINGS] font path: %s, exist=%s", p, rw ? "YES" : "NO");
        if (rw) SDL_RWclose(rw);
    }

    if (!s_titleFont && !G_FontMain) {
        SDL_Log("[SETTINGS] No font available (both title and global NULL)");
    }

    // render() 타이틀 그리기 직전에
    SDL_Log("[SETTINGS] render title try");

}

static void handle(SDL_Event* e)
{
    if (e->type == SDL_QUIT) {
        G_Running = 0;
        return;
    }
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
        scene_switch(SCENE_MAINMENU);
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        scene_switch(SCENE_MAINMENU);
    }
    

}

static void update(float dt) { (void)dt; }

static void render(SDL_Renderer* r) {
    settings_render(r);
    // 0) 매 프레임 시작할 때만 Clear
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    SDL_RenderClear(r);

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
}

static void cleanup(void)
{
    SDL_Log("SETTINGS: cleanup()");
    if (s_bg) { SDL_DestroyTexture(s_bg); s_bg = NULL; }
    if (s_titleFont) { TTF_CloseFont(s_titleFont); s_titleFont = NULL; }
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Settings" };
Scene* scene_settings_object(void)
{
    return &SCENE_OBJ;
}