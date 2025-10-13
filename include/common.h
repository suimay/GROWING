#ifndef COMMON_H
#define COMMON_H

// 빌드 환경별 SDL 인클루드 (vcpkg 기본 구조)
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>


#define APP_TITLE "GROWING"
#define APP_WIDTH  1920
#define APP_HEIGHT 1080

// 경로 헬퍼(실행 폴더 기준)
#define ASSETS_DIR        "assets/"
#define ASSETS_FONTS_DIR  "assets/fonts/"
#define ASSETS_SOUNDS_DIR "assets/sounds/"
#define ASSETS_IMAGES_DIR "assets/images/"

#endif
