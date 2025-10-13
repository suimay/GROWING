#ifndef GAME_H
#define GAME_H
#include "include/scene.h"

extern SDL_Window* G_Window;
extern SDL_Renderer* G_Renderer;
extern int           G_Running;

// 전역 리소스(선택)
extern TTF_Font* G_FontMain;
extern Mix_Music* G_BGM;

int  game_init(void);   // SDL/폰트/오디오/창/렌더러
void game_shutdown(void);

#endif
