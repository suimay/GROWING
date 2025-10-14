#include "../include/ui.h"
#include <stdio.h>

void ui_progress_init(UIProgressBar* pb, SDL_Rect r) {
    pb->r = r;
    pb->value01 = 0.f;
    pb->show_text = 1;
}

void ui_progress_set(UIProgressBar* pb, float v01) {
    if (v01 < 0.f) v01 = 0.f;
    if (v01 > 1.f) v01 = 1.f;
    pb->value01 = v01;
}

void ui_progress_render(SDL_Renderer* ren, TTF_Font* font, const UIProgressBar* pb) {
    // 배경(어두운 회색)
    SDL_SetRenderDrawColor(ren, 40, 45, 55, 255);
    SDL_RenderFillRect(ren, &pb->r);

    // 채움(초록빛)
    SDL_Rect fill = pb->r;
    fill.w = (int)(fill.w * pb->value01);
    SDL_SetRenderDrawColor(ren, 80, 200, 140, 255);
    SDL_RenderFillRect(ren, &fill);

    // 테두리
    SDL_SetRenderDrawColor(ren, 220, 230, 240, 180);
    SDL_RenderDrawRect(ren, &pb->r);

    // 퍼센트 텍스트(선택)
    if (pb->show_text && font) {
        char buf[16];
        int percent = (int)(pb->value01 * 100.0f + 0.5f);
        snprintf(buf, sizeof(buf), "%d%%", percent);

        SDL_Color fg = { 235, 245, 235, 255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, buf, fg);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { pb->r.x + (pb->r.w - tw) / 2, pb->r.y + (pb->r.h - th) / 2, tw, th };
            SDL_RenderCopy(ren, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
}