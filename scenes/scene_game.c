#include "../include/scene.h"
#include <SDL2/SDL.h>

static void game_init(void) {}
static void game_handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) g_running = 0;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) switch_scene(SCENE_MAIN_MENU);
}
static void game_update(float dt) { (void)dt; }
static void game_render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 24, 28, 36, 255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);
}
static void game_cleanup(void) {}

Scene GAME_SCENE = {
    game_init, game_handle, game_update, game_render, game_cleanup
};
