#include "../include/ui.h"

int ui_point_in_rect(int x, int y, const SDL_Rect *r)
{
    return (x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h);
}

void ui_button_init(UIButton *b, SDL_Rect r, const char *text)
{
    b->r = r;
    b->text = text;
    b->enabled = 1;
    b->hovered = 0;
    b->pressed = 0;
    b->sfx_click = NULL;
    b->sfx_hover = NULL;
    b->on_click = NULL;
    b->userdata = NULL;
}

void ui_button_set_callback(UIButton *b, UIButtonOnClick fn, void *userdata)
{
    b->on_click = fn;
    b->userdata = userdata;
}

void ui_button_set_sfx(UIButton *b, Mix_Chunk *clickSfx, Mix_Chunk *hoverSfx)
{
    b->sfx_click = clickSfx;
    b->sfx_hover = hoverSfx;
}

void ui_button_handle(UIButton *b, const SDL_Event *e)
{
    if (!b->enabled)
        return;

    if (e->type == SDL_MOUSEMOTION)
    {
        int hov_prev = b->hovered;
        b->hovered = ui_point_in_rect(e->motion.x, e->motion.y, &b->r);
        if (b->hovered && !hov_prev && b->sfx_hover)
        {
            Mix_PlayChannel(-1, b->sfx_hover, 0);
        }
    }
    else if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
    {
        if (ui_point_in_rect(e->button.x, e->button.y, &b->r))
        {
            b->pressed = 1;
        }
    }
    else if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT)
    {
        int inside = ui_point_in_rect(e->button.x, e->button.y, &b->r);
        if (b->pressed && inside)
        {
            if (b->sfx_click)
                Mix_PlayChannel(-1, b->sfx_click, 0);
            if (b->on_click)
                b->on_click(b->userdata);
        }
        b->pressed = 0;
    }
}
void ui_button_render(SDL_Renderer *ren, TTF_Font *font, const UIButton *b, SDL_Texture *bg)
{
    if (bg)
    {
        // 배경 이미지가 주어졌으면 이미지 렌더링
        SDL_RenderCopy(ren, bg, NULL, &b->r);
    }
    else
    {
        // 아니면 기본 색상 박스
        SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
        SDL_RenderFillRect(ren, &b->r);
    }

    // 텍스트
    /*
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
    */

    if (font && b->text && *b->text)
    {
        SDL_Color c = {255, 255, 255, 255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, b->text, c);
        SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
        int tw, th;
        SDL_QueryTexture(t, NULL, NULL, &tw, &th);
        SDL_Rect dst = {
            b->r.x + (b->r.w - tw) / 2,
            b->r.y + (b->r.h - th) / 2,
            tw, th};
        SDL_RenderCopy(ren, t, NULL, &dst);
        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}
