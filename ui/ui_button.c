#include "../include/ui.h"

int ui_point_in_rect(int x, int y, const SDL_Rect* r) {
    return (x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h);
}

void ui_draw_button(SDL_Renderer* ren, TTF_Font* font, const UIButton* b) {
    SDL_Color base = b->hovered ? (SDL_Color) { 70, 140, 255, 255 } : (SDL_Color) { 50, 90, 170, 255 };
    if (!b->enabled) base = (SDL_Color){ 90,90,90,255 };
    SDL_SetRenderDrawColor(ren, base.r, base.g, base.b, base.a);
    SDL_RenderFillRect(ren, &b->r);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 180);
    SDL_RenderDrawRect(ren, &b->r);

    if (font && b->text) {
        SDL_Color fg = { 230,230,230,255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, b->text, fg);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
            int w, h; SDL_QueryTexture(t, NULL, NULL, &w, &h);
            SDL_Rect d = { b->r.x + (b->r.w - w) / 2, b->r.y + (b->r.h - h) / 2, w, h };
            SDL_RenderCopy(ren, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
}
