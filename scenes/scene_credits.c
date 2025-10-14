#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"

static float scroll_y = 0.0f;
static float scroll_speed = 30.0f; // 초당 픽셀
static int fading_in = 1;
static float fade_alpha = 255.0f;

typedef struct {
    const char* line;
} CreditLine;

static CreditLine lines[] = {
    {" PROJECT GROWING "},
    {" "},
    {"Directed & Programmed by"},
    {"김기태 (Dollemi3)"},
    {"이수인 (SUI)"},
    {" "},
    {"Art & Design"},
    {"김기태, 이수인"},
    {"SDL2 UI / Plant Concept"},
    {" "},
    {"Music"},
    {"Main Theme: 'Morning twilight' 김기태"},
    {"Theme2 : '???' ??? "},
    {"Theme3 : '???' ??? "},
    {"Theme4 : '???' ??? "},
    {"Theme5 : '???' ??? "},
    {"Sound Effects: SDL2 Mixer Pack"},
    {" "},
    {"Special Thanks"},
    {"Team Member"},
    {"김기태"},
    {"이수인"},
    {"Professor [*정현준 교수님*]"},
    {" "},
    {"Made with SDL2 / C Language"},
    {"2025 © Growing Team. All Rights Reserved."},
    {NULL}
};

static void init(void)
{
    scroll_y = APP_HEIGHT + 50; // 처음은 아래에서 시작
    fade_alpha = 255.0f;
    fading_in = 1;
    SDL_Log("CREDITS: init()");
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
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_DOWN) {
        scroll_speed = 120.0f;
    }
    if (e->type == SDL_KEYUP && e->key.keysym.sym == SDLK_DOWN) {
        scroll_speed = 30.0f;
    }

}

static void update(float dt)
{
    if (fading_in) {
        fade_alpha -= 200.0f * dt;
        if (fade_alpha <= 0) {
            fade_alpha = 0;
            fading_in = 0;
        }
    }

    scroll_y -= scroll_speed * dt;
    if (scroll_y < -1100) { // 화면 다 올라가면 자동 복귀
        scene_switch_fade(SCENE_MAINMENU,0.35f,0.6f);
    }
}

static void render(SDL_Renderer* r)
{
    SDL_SetRenderDrawColor(r, 10, 15, 25, 255);
   

    if (!G_FontMain) return;

    int i = 0;
    float y = scroll_y;
    SDL_Color color = { 220, 230, 230, 255 };

    while (lines[i].line) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(G_FontMain, lines[i].line, color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            int w, h; SDL_QueryTexture(tex, NULL, NULL, &w, &h);
            SDL_Rect dst = { (APP_WIDTH - w) / 2, (int)y, w, h };
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }
        y += 40; // 줄 간격
        i++;
    }

    // 페이드 인 효과
    if (fading_in && fade_alpha > 0) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)fade_alpha);
        SDL_Rect full = { 0, 0, APP_WIDTH, APP_HEIGHT };
        SDL_RenderFillRect(r, &full);
    }

    

   
}

static void cleanup(void)
{
    SDL_Log("CREDITS: cleanup()");
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Credits" };
Scene* scene_credits_object(void)
{
    return &SCENE_OBJ;
}