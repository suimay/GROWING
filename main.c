#include "game.h"
#include "scene_manager.h"
#include "include/common.h"
#include "include/loading.h"
#include "include/settings.h"

// 씬 “팩토리” 프로토타입
Scene *scene_mainmenu_object(void);
Scene *scene_gameplay_object(void);
Scene *scene_settings_object(void);
Scene *scene_codex_object(void);
// Scene* scene_stats_object(void);
Scene *scene_credits_object(void);
Scene *scene_selectplant_object(void);
Scene *scene_plantinfo_object(void);
int main(void)
{
    if (!game_init())
        return 1;
    if (!settings_load())
    {
        SDL_Log("Settings load failed, using defaults.");
    }


    settings_apply_audio();
    system("python assets/weather_update.py");
    SDL_Log("날씨 업데이트 1회 완료");
    // 창 모드 적용 예시 (원하면)
    // if (G_Settings.video.fullscreen) SDL_SetWindowFullscreen(G_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    // SDL_SetWindowSize(G_Window, G_Settings.video.width, G_Settings.video.height);
    //  씬 등록
    scene_register(SCENE_LOADING, scene_loading_object());
    scene_register(SCENE_MAINMENU, scene_mainmenu_object());
    scene_register(SCENE_GAMEPLAY, scene_gameplay_object());
    scene_register(SCENE_SETTINGS, scene_settings_object());
    scene_register(SCENE_PLANTINFO, scene_plantinfo_object());
    scene_register(SCENE_CODEX, scene_codex_object());
    // scene_register(SCENE_STATS,    scene_stats_object())
    scene_register(SCENE_SELECT_PLANT, scene_selectplant_object());
    scene_register(SCENE_CREDITS, scene_credits_object());

    scene_switch(SCENE_MAINMENU);

    Uint64 last = SDL_GetPerformanceCounter(), now;
    double freq = (double)SDL_GetPerformanceFrequency();

    while (G_Running)
    {

        now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / freq);
        last = now;

        SDL_Event e;

        while (SDL_PollEvent(&e))
            scene_handle(&e);
        scene_update(dt);

        SDL_SetRenderDrawColor(G_Renderer, 16, 20, 28, 255);
        SDL_RenderClear(G_Renderer);
        // draw current scene (clear/Present 금지)
        scene_render(G_Renderer);

        // (선택) 페이드/오버레이가 있으면 여기서 그리기

        // present (여기서 한 번만)
        SDL_RenderPresent(G_Renderer);
    }

    scene_cleanup();
    game_shutdown();
    return 0;
}
