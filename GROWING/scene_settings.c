// scene_settings.c
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include "scene.h"   // Scene 구조체, switch_scene() 선언 포함

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
    bgm_slider.value = 0.7f;
    bgm_slider.dragging = 0;

    // SFX 슬라이더
    sfx_slider.bar = (SDL_Rect){ cx - 200, baseY + 100, 400, 10 };
    sfx_slider.handle = (SDL_Rect){ cx - 10, baseY + 95, 20, 20 };
    sfx_slider.value = 0.8f;
    sfx_slider.dragging = 0;

    // 버튼
    btn_test.rc = (SDL_Rect){ cx - 150, baseY + 200, 120, 40 };
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
        g_running = 0;
        break;
    case SDL_KEYDOWN:
        if (e->key.keysym.sym == SDLK_ESCAPE)
            switch_scene(SCENE_MAIN_MENU);
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (e->button.button == SDL_BUTTON_LEFT) {
            // BGM 슬라이더
            if (point_in_rect(mx, my, bgm_slider.handle)) bgm_slider.dragging = 1;
            // SFX 슬라이더
            else if (point_in_rect(mx, my, sfx_slider.handle)) sfx_slider.dragging = 1;
            // 효과음 테스트
            else if (point_in_rect(mx, my, btn_test.rc)) {
                Mix_PlayChannel(-1, sfx_click, 0);
            }
            // 뒤로가기
            else if (point_in_rect(mx, my, btn_back.rc)) {
                switch_scene(SCENE_MAIN_MENU);
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
    SDL_RenderClear(r);

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

    SDL_RenderPresent(r);
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

// ---------------- Scene 구조체 ----------------
Scene SETTINGS_SCENE = {
    settings_init,
    settings_handle,
    settings_update,
    settings_render,
    settings_cleanup
};
