#include "game.h"
#include "scene_manager.h"
#include "include/common.h"

// 씬 “팩토리” 프로토타입
Scene* scene_mainmenu_object(void);
//Scene* scene_gameplay_object(void);
// Scene* scene_settings_object(void);
// Scene* scene_codex_object(void);
// Scene* scene_stats_object(void);
Scene* scene_credits_object(void);

int main(void) {
    if (!game_init()) return 1;

    // 씬 등록
    scene_register(SCENE_MAINMENU, scene_mainmenu_object());
    //scene_register(SCENE_GAMEPLAY, scene_gameplay_object());
    // scene_register(SCENE_SETTINGS, scene_settings_object());
    // scene_register(SCENE_CODEX,    scene_codex_object());
    // scene_register(SCENE_STATS,    scene_stats_object());
     scene_register(SCENE_CREDITS,  scene_credits_object());

    scene_switch(SCENE_MAINMENU);

    Uint64 last = SDL_GetPerformanceCounter(), now;
    double freq = (double)SDL_GetPerformanceFrequency();

    while (G_Running) {
        now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / freq); last = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) scene_handle(&e);
        scene_update(dt);
        scene_render(G_Renderer);
        SDL_RenderPresent(G_Renderer);
    }

    scene_cleanup();
    game_shutdown();
    return 0;
}
