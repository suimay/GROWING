#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"

#define MAX_COL 2
static int s_rows = 0, s_cols = MAX_COL;
static int s_topIndex = 0;      // 스크롤용
static Mix_Chunk* sfx_click = NULL;
static UIButton s_btnBack;
static SDL_Texture* select_background = NULL;
static SDL_Texture* i_back = NULL;
static SDL_Texture* start_grid = NULL;
static void on_back(void* ud) { (void)ud; scene_switch(SCENE_MAINMENU); }

static void choose_plant(int idx) {
    G_SelectedPlantIndex = idx;
    SDL_Log("Selected plant idx=%d name=%s", idx, plantdb_get(idx)->name_kr);
    scene_switch_fade(SCENE_GAMEPLAY, 0.2f, 0.4f);
}


static void init(void) {
    if (!sfx_click) sfx_click = Mix_LoadWAV(ASSETS_SOUNDS_DIR "click.wav");
    // JSON 로드
    int n = plantdb_load(ASSETS_DIR "plants.json");
    if (n <= 0) SDL_Log("WARNING: plants.json empty or fail");

    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    ui_button_init(&s_btnBack, (SDL_Rect) { 50, 40, 70, 70 }, "");
    ui_button_set_callback(&s_btnBack, on_back, NULL);
    ui_button_set_sfx(&s_btnBack, sfx_click, NULL);

    s_rows = (n + s_cols - 1) / s_cols;
    s_topIndex = 0;

    select_background = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
    if (!select_background) {
        SDL_Log("Load select_Background.png failed: %s", IMG_GetError());
    }

    i_back = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
    if (!i_back) {
        SDL_Log("Losd I_back.png failed: %s", IMG_GetError());
    }

    start_grid = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "start_grid.png");
    if (!i_back) {
        SDL_Log("Losd start_grid.png failed: %s", IMG_GetError());
    }
}

static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { G_Running = 0; return; }
    ui_button_handle(&s_btnBack, e);

    // 마우스 클릭으로 항목 선택
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
        int gridX = 160, gridY = 100, cellW = (w - 200) / s_cols, cellH = 100, pad = 12;

        int mx = e->button.x, my = e->button.y;
        for (int r = 0;r < 6;r++) { // 화면에 보이는 최대 6행(가변이면 계산)
            for (int c = 0;c < s_cols;c++) {
                int idx = (s_topIndex + r) * s_cols + c;
                if (idx >= plantdb_count()) break;
                SDL_Rect cell = { gridX + c * (cellW + pad), gridY + r * (cellH + pad), cellW, cellH };
                if (ui_point_in_rect(mx, my, &cell)) {
                    if (sfx_click) Mix_PlayChannel(-1, sfx_click, 0);
                    choose_plant(idx);
                    return;
                }
            }
        }
    }

    // 휠 스크롤
    if (e->type == SDL_MOUSEWHEEL) {
        int rows_visible = 6;
        int maxTop = SDL_max(0, s_rows - rows_visible);
        s_topIndex -= (e->wheel.y > 0) ? 1 : (e->wheel.y < 0 ? -1 : 0);
        if (s_topIndex < 0) s_topIndex = 0;
        if (s_topIndex > maxTop) s_topIndex = maxTop;
    }
}

static void update(float dt) { (void)dt; }

static void render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    
    if (select_background) {
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        SDL_Rect dst = { 0,0,w,h };
        SDL_RenderCopy(r, select_background, NULL, &dst);
    }
    else {
        int w, h; SDL_GetRendererOutputSize(r, &w, &h);
        for (int y = 0; y < h; ++y) {
            float t = (float)y / (float)h;
            Uint8 R = (Uint8)(16 + t * 20);
            Uint8 G = (Uint8)(20 + t * 60);
            Uint8 B = (Uint8)(28 + t * 40);
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }
    // 타이틀
    if (G_FontMain) {
        SDL_Color c = { 230,240,235,255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(G_FontMain, "키울 식물을 선택하세요", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            int w, h; SDL_GetRendererOutputSize(r, &w, &h);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { (w - tw) / 2, 30, tw, th };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }

    // 그리드
    SDL_Rect dst = { 280,100 , 268 * 5, 187 * 5 };
    SDL_Rect grid = { 0,0 , 268 , 187 };
    SDL_RenderCopy(r, start_grid, &grid, &dst);

    int w, h; SDL_GetRendererOutputSize(r, &w, &h);
    int gridX = 160, gridY = 100, cellW = (w - 500) / s_cols, cellH = 100, pad = 12;

    for (int rrow = 0; rrow < 6; rrow++) {
        for (int c = 0;c < s_cols;c++) {
            int idx = (s_topIndex + rrow) * s_cols + c;
            if (idx >= plantdb_count()) break;

            const PlantInfo* p = plantdb_get(idx);
            SDL_Rect cell = { gridX + c * (cellW + pad), gridY + rrow * (cellH + pad), cellW, cellH };

            // 카드 배경

            SDL_SetRenderDrawColor(r, 40, 60, 95, 255);
            SDL_RenderFillRect(r, &cell);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 100);
            SDL_RenderDrawRect(r, &cell);
            /*

            // 텍스트(이름 / 라틴명)
            if (G_FontMain) {
                SDL_Color c1 = { 235,245,245,255 }, c2 = { 200,210,220,255 };
                // 이름
                SDL_Surface* s1 = TTF_RenderUTF8_Blended(G_FontMain, p->name_kr, c1);
                if (s1) {
                    SDL_Texture* t1 = SDL_CreateTextureFromSurface(r, s1);
                    int tw, th; SDL_QueryTexture(t1, NULL, NULL, &tw, &th);
                    SDL_Rect d1 = { cell.x + 14, cell.y + 14, tw, th };
                    SDL_RenderCopy(r, t1, NULL, &d1);
                    SDL_DestroyTexture(t1); SDL_FreeSurface(s1);
                }
                // 라틴명(작게)
                char latin[96]; SDL_snprintf(latin, sizeof(latin), "%s", p->latin);
                SDL_Surface* s2 = TTF_RenderUTF8_Blended(G_FontMain, latin, c2);
                if (s2) {
                    SDL_Texture* t2 = SDL_CreateTextureFromSurface(r, s2);
                    int tw, th; SDL_QueryTexture(t2, NULL, NULL, &tw, &th);
                    SDL_Rect d2 = { cell.x + 14, cell.y + 44, tw, th };
                    SDL_RenderCopy(r, t2, NULL, &d2);
                    SDL_DestroyTexture(t2); SDL_FreeSurface(s2);
                }
            }
            */
        }
    }

    // 뒤로 버튼
    ui_button_render(r, G_FontMain, &s_btnBack, i_back);
}

static void cleanup(void) {
    if (sfx_click) { Mix_FreeChunk(sfx_click); sfx_click = NULL; }
    //plantdb_free();  // 선택 유지하려면 여기서 즉시 free하지 말고, 프로그램 종료 때 정리
}


static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "SelectPlant" };
Scene* scene_selectplant_object(void) { return &SCENE_OBJ; }
