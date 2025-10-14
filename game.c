#include "game.h"
#include "include/common.h"

SDL_Window* G_Window = NULL;
SDL_Renderer* G_Renderer = NULL;
int           G_Running = 1;
int G_SelectedPlantIndex = -1;
TTF_Font* G_FontMain = NULL;
Mix_Music* G_BGM = NULL;


extern void plantdb_free(void);
Mix_Chunk* G_SFX_Click = NULL;
Mix_Chunk* G_SFX_Hover = NULL;

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

    G_Renderer = SDL_CreateRenderer(G_Window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!G_Renderer) { SDL_Log("CreateRenderer: %s", SDL_GetError()); return 0; }

    // 기본 폰트/BGM
    G_FontMain = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 28);
    if (!G_FontMain) SDL_Log("TTF_OpenFont: %s", TTF_GetError());

    G_BGM = Mix_LoadMUS(ASSETS_SOUNDS_DIR "MAINSCENEBGM.wav");
    if (!G_BGM) SDL_Log("LoadMUS: %s", Mix_GetError());
    else { Mix_VolumeMusic((int)(0.7f * MIX_MAX_VOLUME)); Mix_PlayMusic(G_BGM, -1); }
        
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

