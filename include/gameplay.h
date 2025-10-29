#pragma once
#include <SDL2/SDL.h>

typedef struct LoadCtx {
    int    step;
    Uint32 t0;
} LoadCtx;

extern LoadCtx s_loadCtx;   // 선언
int gameplay_loading_job(void* u, float* out_p);
