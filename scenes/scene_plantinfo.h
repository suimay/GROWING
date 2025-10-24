#ifndef SCENE_PLANTINFO_H
#define SCENE_PLANTINFO_H

#include <stdbool.h>
#include <SDL2/SDL.h>

bool plantinfo_init(void *arg);
void plantinfo_handle_event(const SDL_Event *e);
void plantinfo_update(float dt);
void plantinfo_render(void);
void plantinfo_cleanup(void);

struct Scene;
struct Scene *scene_plantinfo_object(void);

#endif /* SCENE_PLANTINFO_H */
