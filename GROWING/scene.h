#pragma once
// scene.h
#ifndef SCENE_H
#define SCENE_H

#include <SDL2/SDL.h>

// ======================
// 🎯 씬 기본 구조체 정의
// ======================
typedef struct Scene {
    void (*init)(void);                 // 씬 초기화
    void (*handle_event)(SDL_Event* e); // 이벤트 처리
    void (*update)(float dt);           // 업데이트 (delta time)
    void (*render)(SDL_Renderer* r);    // 렌더링
    void (*cleanup)(void);              // 리소스 정리
} Scene;

// ======================
// 🎬 전역 공유 변수
// ======================
extern SDL_Window* g_win;       // 윈도우 핸들
extern SDL_Renderer* g_ren;       // 렌더러 핸들
extern int           g_running;   // 게임 루프 유지 여부
extern Scene         g_scene;     // 현재 활성화된 씬

// ======================
// 🎮 씬 ID 정의
// ======================
typedef enum {
    SCENE_MAIN_MENU = 0,
    SCENE_GAME,
    SCENE_COLLECTION,
    SCENE_SETTINGS,
    SCENE_QUIT
} SceneID;

// ======================
// 🚀 공통 함수
// ======================

// 씬 전환 함수 (예: switch_scene(SCENE_SETTINGS))
void switch_scene(SceneID next);

// ======================
// 🌿 외부 씬 선언 (각 .c 파일에서 구현)
// ======================
extern Scene MAIN_MENU;
extern Scene GAME_SCENE;
extern Scene COLLECTION_SCENE;
extern Scene SETTINGS_SCENE;

#endif // SCENE_H
