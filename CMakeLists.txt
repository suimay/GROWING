cmake_minimum_required(VERSION 3.16)
project(GROWING C)
set(CMAKE_C_STANDARD 11)

# 소스 경로: 윈도우 솔루션 안쪽 폴더(GROWING/)에 C 파일들이 있음
file(GLOB SRC "GROWING/*.c")

add_executable(growing ${SRC})

# SDL2 (Homebrew 설치 + pkg-config 활용)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_MIXER REQUIRED sdl2_mixer)
pkg_check_modules(SDL2_TTF REQUIRED sdl2_ttf)

target_include_directories(growing PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_directories(growing PRIVATE 
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_MIXER_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
)

target_compile_options(growing PRIVATE 
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_MIXER_CFLAGS_OTHER}
    ${SDL2_TTF_CFLAGS_OTHER}
)

target_link_libraries(growing 
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)

target_compile_definitions(growing PRIVATE SDL_MAIN_HANDLED)


# 윈도우용 SDL2main 충돌 방지 (윈도우에서 SDL_main 사용했다면)
target_compile_definitions(growing PRIVATE SDL_MAIN_HANDLED)

# 실행 시 현재 작업 폴더를 프로젝트 루트로 가정(assets 상대경로 쓸 경우)
