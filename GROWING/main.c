// main.c — GROWING 프로젝트 (SDL2 + Scene 구조 + 오디오 + 폰트)
// ---------------------------------------------------------------
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include "scene.h"     // 공통 Scene 시스템 헤더

// ---------------- 전역 정의 (scene.h에서 extern으로 선언됨) ----------------
SDL_Window* g_win = NULL;
SDL_Renderer* g_ren = NULL;
int           g_running = 1;
Scene         g_scene;          // 현재 씬
SceneID       g_scene_id = SCENE_MAIN_MENU;
static Mix_Music* g_bgm = NULL;   // BGM 핸들

// ---------------- 내부 전용 ----------------
static SDL_AudioDeviceID g_audio_dev = 0;
static Uint8* g_wav_buffer = NULL;
static Uint32 g_wav_length = 0;

// ---------------- Scene 전환 ----------------
void switch_scene(SceneID next) {
    if (g_scene.cleanup) g_scene.cleanup();

    g_scene_id = next;
    switch (next) {
    case SCENE_MAIN_MENU:   g_scene = MAIN_MENU;         break;
    case SCENE_GAME:        g_scene = GAME_SCENE;        break;
    case SCENE_COLLECTION:  g_scene = COLLECTION_SCENE;  break;
    case SCENE_SETTINGS:    g_scene = SETTINGS_SCENE;    break;
    case SCENE_QUIT:        g_running = 0;               return;
    default:                g_scene = MAIN_MENU;         break;
    }

    if (g_scene.init) g_scene.init();
}

// ---------------- 오디오 초기화 ----------------
static int init_audio(void) {
    // BGM 로드
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Mix_OpenAudio Error:%s\n", Mix_GetError());
    }

    g_bgm = Mix_LoadMUS("assets/MAINSCENEBGM.wav");
    if (!g_bgm) {
        printf("LoadMUS Error: %s\n", Mix_GetError());
    }
    else {
        Mix_VolumeMusic((int)(0.5f * MIX_MAX_VOLUME));
        Mix_PlayMusic(g_bgm, -1);
    }

    if (Mix_VolumeMusic == 0) {
        printf("음악 꺼짐");
    }

    return 1;
}

// ---------------- 메인 엔트리 ----------------
int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // 1️⃣ SDL 초기화
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // 2️⃣ 창 & 렌더러 생성
    g_win = SDL_CreateWindow(
        "GROWING - Main Menu",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 540, SDL_WINDOW_RESIZABLE);
    if (!g_win) {
        printf("CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    g_ren = SDL_CreateRenderer(g_win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) {
        printf("CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_win);
        SDL_Quit();
        return 1;
    }

    // 3️⃣ 오디오 초기화 (배경음)
    init_audio();

    // 4️⃣ 첫 번째 씬: 메인 메뉴
    switch_scene(SCENE_MAIN_MENU);

    // 5️⃣ 메인 루프
    Uint64 last = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();

    while (g_running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / freq);
        last = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (g_scene.handle_event) g_scene.handle_event(&e);
        }

        if (g_scene.update) g_scene.update(dt);
        if (g_scene.render) g_scene.render(g_ren);

        // 🎵 배경음 루프
        if (g_audio_dev && SDL_GetQueuedAudioSize(g_audio_dev) < (g_wav_length / 4)) {
            SDL_QueueAudio(g_audio_dev, g_wav_buffer, g_wav_length);
        }
    }

    // 6️⃣ 정리
    if (g_scene.cleanup) g_scene.cleanup();

    if (g_bgm) { Mix_HaltMusic(); Mix_FreeMusic(g_bgm); g_bgm = NULL; };

    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}
