#include "game.h"
#include "include/common.h"
#include <errno.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define CHDIR _chdir
#else
#include <unistd.h>
#define CHDIR chdir
#endif

SDL_Window* G_Window = NULL;
SDL_Renderer* G_Renderer = NULL;
int           G_Running = 1;
int G_SelectedPlantIndex = -1;
TTF_Font* G_FontMain = NULL;
Mix_Music* G_BGM = NULL;


extern void plantdb_free(void);
Mix_Chunk* G_SFX_Click = NULL;
Mix_Chunk* G_SFX_Hover = NULL;

// 실행 위치가 바뀌어도 assets 경로를 맞춰주기 위해 실행 파일 위치로 작업 디렉터리 이동
static void ensure_workdir_at_exe(void) {
    char* base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("[PATH] SDL_GetBasePath failed: %s", SDL_GetError());
        return;
    }
    if (CHDIR(base) != 0) {
        SDL_Log("[PATH] chdir(%s) failed: %s", base, strerror(errno));
        SDL_free(base);
        return;
    }
    SDL_Log("[PATH] cwd set to %s", base);
    SDL_free(base);
}

int game_init(void) {

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError()); return 0;
    }
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init: %s", TTF_GetError()); return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio: %s", Mix_GetError()); return 0;
    }

    G_Window = SDL_CreateWindow(APP_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        APP_WIDTH, APP_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!G_Window) { SDL_Log("CreateWindow: %s", SDL_GetError()); return 0; }

    // 실행 파일 위치 기준으로 assets를 찾도록 작업 디렉터리 고정
    ensure_workdir_at_exe();

    G_Renderer = SDL_CreateRenderer(G_Window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!G_Renderer) { SDL_Log("CreateRenderer: %s", SDL_GetError()); return 0; }

    // 기본 폰트/BGM
    G_FontMain = TTF_OpenFont(ASSETS_FONTS_DIR "NeoDunggeunmoPro-Regular.ttf", 28);
    if (!G_FontMain) SDL_Log("TTF_OpenFont: %s", TTF_GetError());

    /*
    G_BGM = Mix_LoadMUS(ASSETS_SOUNDS_DIR "MAINSCENEBGM.wav");
    if (!G_BGM) SDL_Log("LoadMUS: %s", Mix_GetError());
    else { Mix_VolumeMusic((int)(0.7f * MIX_MAX_VOLUME)); Mix_PlayMusic(G_BGM, -1); }
    */
        
    G_SFX_Click = Mix_LoadWAV(ASSETS_SOUNDS_DIR "click.wav");
    G_SFX_Hover = Mix_LoadWAV(ASSETS_SOUNDS_DIR "move.wav");

    return 1;
}

void game_shutdown(void) {
    if (G_SFX_Click) { Mix_FreeChunk(G_SFX_Click); G_SFX_Click = NULL; }
    if (G_SFX_Hover) { Mix_FreeChunk(G_SFX_Hover); G_SFX_Hover = NULL; }

    plantdb_free();
    Mix_CloseAudio();
    TTF_Quit();
    SDL_Quit();
}
