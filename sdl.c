#include <SDL2/SDL.h>
#include <stdbool.h>

// 게임 상태
typedef enum {
    STATE_MAIN_MENU,
    STATE_PLAY,
    STATE_SETTINGS,
    STATE_EXIT
} GameState;

// 프로토타입
void renderMainMenu(SDL_Renderer *renderer);
void renderGame(SDL_Renderer *renderer);
void renderSettings(SDL_Renderer *renderer);
void handleMainMenuEvent(SDL_Event *e, GameState *state);
void handleGameEvent(SDL_Event *e, GameState *state);

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window *window = SDL_CreateWindow(
        "GROWING",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 540,  // 논리 해상도320x180, 화면은 3배(뻥튀기)960x540
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE //user가 창크기 조절할 수 있게
    );
    if (!window) return 1;

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return 1;

    // 픽셀아트 기본 세팅
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // Nearest
    SDL_RenderSetLogicalSize(renderer, 320, 180);    // 논리 해상도
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);   // 정수배 스케일 + 레터박스 -> 픽셀 또려하게 유지(여백메움)

    bool running = true;
    GameState state = STATE_MAIN_MENU;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            switch (state) {
                case STATE_MAIN_MENU:
                    handleMainMenuEvent(&e, &state);
                    break;
                case STATE_PLAY:
                    handleGameEvent(&e, &state);
                    break;
                case STATE_SETTINGS:
                    // TODO: 설정 이벤트
                    break;
                default: break;
            }
        }

        // 상태별 렌더
        switch (state) {
            case STATE_MAIN_MENU:
                renderMainMenu(renderer);
                break;
            case STATE_PLAY:
                renderGame(renderer);
                break;
            case STATE_SETTINGS:
                renderSettings(renderer);
                break;
            case STATE_EXIT:
                running = false;
                break;
            default: break;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60fps
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

// ───────────── 화면 렌더링 ─────────────

void renderMainMenu(SDL_Renderer *renderer) {
    // 배경
    SDL_SetRenderDrawColor(renderer, 30, 36, 40, 255);
    SDL_RenderClear(renderer);

    // 타이틀 바
    SDL_Rect title = {60, 20, 200, 30};
    SDL_SetRenderDrawColor(renderer, 80, 180, 120, 255);
    SDL_RenderFillRect(renderer, &title);

    // 버튼 4개
    SDL_Rect btnStart    = {100, 70, 120, 26};
    SDL_Rect btnContinue = {100, 100, 120, 26};
    SDL_Rect btnSettings = {100, 130, 120, 26};
    SDL_Rect btnExit     = {100, 160, 120, 26};

    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(renderer, &btnStart);
    SDL_RenderFillRect(renderer, &btnContinue);
    SDL_RenderFillRect(renderer, &btnSettings);

    SDL_SetRenderDrawColor(renderer, 200, 80, 80, 255);
    SDL_RenderFillRect(renderer, &btnExit);

    // 테두리
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderDrawRect(renderer, &btnStart);
    SDL_RenderDrawRect(renderer, &btnContinue);
    SDL_RenderDrawRect(renderer, &btnSettings);
    SDL_RenderDrawRect(renderer, &btnExit);
}

void renderGame(SDL_Renderer *renderer) {
    // 방 배경
    SDL_SetRenderDrawColor(renderer, 220, 220, 225, 255);
    SDL_RenderClear(renderer);

    // 창문(날씨 영역)
    SDL_Rect windowRect = {80, 20, 160, 50};
    SDL_SetRenderDrawColor(renderer, 180, 220, 255, 255);
    SDL_RenderFillRect(renderer, &windowRect);
    SDL_SetRenderDrawColor(renderer, 40, 60, 80, 255);
    SDL_RenderDrawRect(renderer, &windowRect);

    // 식물(화분 + 줄기 자리)
    SDL_Rect pot   = {144, 120, 32, 20};
    SDL_Rect plant = {152, 70, 16, 50};
    SDL_SetRenderDrawColor(renderer, 120, 60, 40, 255);  SDL_RenderFillRect(renderer, &pot);
    SDL_SetRenderDrawColor(renderer, 90, 160, 80, 255);  SDL_RenderFillRect(renderer, &plant);

    // 하단 바(버튼 자리)
    SDL_Rect bottomBar = {0, 160, 320, 20};
    SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
    SDL_RenderFillRect(renderer, &bottomBar);
}

void renderSettings(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderClear(renderer);
}

// ───────────── 입력 처리 ─────────────

void handleMainMenuEvent(SDL_Event *e, GameState *state) {
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_1: *state = STATE_PLAY;     break; // 바로 게임 시작
            case SDLK_2: /* 이어하기 */           break;
            case SDLK_3: *state = STATE_SETTINGS; break;
            case SDLK_4: *state = STATE_EXIT;     break;
            default: break;
        }
    }
}

void handleGameEvent(SDL_Event *e, GameState *state) {
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE: *state = STATE_MAIN_MENU; break;
            case SDLK_w: /* 물 주기 */    break;
            case SDLK_s: /* 햇빛/위치 */  break;
            case SDLK_f: /* 비료 */      break;
            default: break;
        }
    }
}
