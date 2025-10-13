#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H
#include "include/scene.h"

extern Scene* G_Scenes[SCENE__COUNT];
extern SceneID  G_CurrentScene;

void scene_register(SceneID id, Scene* sc);
void scene_switch(SceneID id);

void scene_switch_fade(SceneID target, float out_sec, float in_sec);

void scene_handle(SDL_Event* e);
void scene_update(float dt);

void scene_render(SDL_Renderer* r);

int scene_is_transitioning(void);

void scene_cleanup(void);

#endif
