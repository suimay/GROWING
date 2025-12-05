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
void ui_button_set_icons(UIButton* b,
    SDL_Texture* normal,
    SDL_Texture* hover,
    SDL_Texture* pressed)
{
    b->tex_normal = normal;
    b->tex_hover = hover;
    b->tex_pressed = pressed;
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

void ui_button_render(SDL_Renderer* ren, TTF_Font* font, const UIButton* b, SDL_Texture* override_bg)
{
    // 1) 어떤 텍스처를 쓸지 상태에 따라 결정
    SDL_Texture* tex = override_bg;  // 매개변수로 강제 배경이 들어온 경우 최우선

    if (!tex) {
        // override 없으면 버튼 상태를 보고 선택
        if (b->pressed && b->tex_pressed) {
            tex = b->tex_pressed;
        }
        else if (b->hovered && b->tex_hover) {
            tex = b->tex_hover;
        }
        else if (b->tex_normal) {
            tex = b->tex_normal;
        }
        else if (b->bg_texture) {
            tex = b->bg_texture;
        }
    }

    // 2) 텍스처 있으면 그거 사용, 없으면 색 박스
    if (tex) {
        SDL_RenderCopy(ren, tex, NULL, &b->r);
    }
    else {
        // 상태에 따른 색만 살짝 바꿔도 됨
        if (!b->enabled) {
            SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        }
        else if (b->pressed) {
            SDL_SetRenderDrawColor(ren, 40, 100, 160, 255);
        }
        else if (b->hovered) {
            SDL_SetRenderDrawColor(ren, 70, 140, 255, 255);
        }
        else {
            SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
        }
        SDL_RenderFillRect(ren, &b->r);
    }

    // 3) 텍스트는 그대로 중앙 정렬
    if (font && b->text && *b->text) {
        SDL_Color c = { 255, 255, 255, 255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, b->text, c);
        if (!s) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
        int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
        SDL_Rect dst = {
            b->r.x + (b->r.w - tw) / 2,
            b->r.y + (b->r.h - th) / 2,
            tw, th
        };
        SDL_RenderCopy(ren, t, NULL, &dst);
        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}

