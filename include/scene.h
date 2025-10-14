#ifndef SCENE_H
#define SCENE_H
#include "common.h"

typedef struct Scene {
    void (*init)(void);
    void (*handle)(SDL_Event*);
    void (*update)(float dt);
    void (*render)(SDL_Renderer*);
    void (*cleanup)(void);
    const char* name;
    char debug_name;
} Scene;

typedef enum {
    SCENE_MAINMENU = 0,
    SCENE_GAMEPLAY,
    SCENE_SETTINGS,
    SCENE_CODEX,
    SCENE_STATS,
    SCENE_CREDITS,
    SCENE_LOADING,
    SCENE_SELECT_PLANT,   
    SCENE__COUNT          
} SceneID;

#endif
