#ifndef UI_H
#define UI_H
#include "common.h"

typedef struct {
    SDL_Rect r;
    const char* text;
    int enabled;
    int hovered;
} UIButton;

int  ui_point_in_rect(int x, int y, const SDL_Rect* r);
void ui_draw_button(SDL_Renderer* ren, TTF_Font* font, const UIButton* b);

#endif
