#include "scene.h"
#include <SDL2/SDL.h>

static void col_init(void) {}
static void col_handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) g_running = 0;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) switch_scene(SCENE_MAIN_MENU);
}
static void col_update(float dt) { (void)dt; }
static void col_render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 30, 34, 42, 255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);
}
static void col_cleanup(void) {}

Scene COLLECTION_SCENE = {
    col_init, col_handle, col_update, col_render, col_cleanup
};
