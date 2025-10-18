#ifndef UI_H
#define UI_H
#include "common.h"

typedef void (*UIButtonOnClick)(void* userdata);

typedef struct {
    SDL_Rect r;
    const char* text;
    int enabled;
    int hovered;
    int pressed;

    Mix_Chunk* sfx_click;
    Mix_Chunk* sfx_hover;

    UIButtonOnClick on_click;
    void* userdata;

    SDL_Texture* bg_texture;
} UIButton;

typedef struct {
    SDL_Rect r;        // 위치/크기
    float value01;     // 0.0 ~ 1.0
    int show_text;     // 퍼센트 텍스트 표시 여부
} UIProgressBar;

void ui_progress_init(UIProgressBar* pb, SDL_Rect r);
void ui_progress_set(UIProgressBar* pb, float v01);
void ui_progress_render(SDL_Renderer* ren, TTF_Font* font, const UIProgressBar* pb);



int  ui_point_in_rect(int x, int y, const SDL_Rect* r);

void ui_button_init(UIButton* b, SDL_Rect r, const char* text);
void ui_button_set_callback(UIButton* b, UIButtonOnClick fn, void* userdata);
void ui_button_set_sfx(UIButton* b, Mix_Chunk* clickSfx, Mix_Chunk* hoverSfx);

void ui_button_handle(UIButton* b, const SDL_Event* e);
void ui_button_render(SDL_Renderer* ren, TTF_Font* font, const UIButton* b, SDL_Texture* bg);

#endif
