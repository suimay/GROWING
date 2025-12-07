// anim_util.h
#pragma once
#include <SDL2/SDL.h>

typedef struct {
    SDL_Rect rect;       // 프레임 위치/크기
    int      duration_ms;// 이 프레임 유지 시간(ms)
} plantFrame;
