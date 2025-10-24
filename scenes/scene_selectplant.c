#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"
#include <stdint.h>

#define MAX_COL 2
#define GRID_VISIBLE_ROWS 6
#define GRID_BASE_WIDTH 268.0f
#define GRID_BASE_HEIGHT 187.0f
#define GRID_ROW_START 1
#define GRID_ROW_STEP 32
#define GRID_SLOT_WIDTH 90.0f
#define GRID_SLOT_HEIGHT 24.0f
#define GRID_SLOT_LEFT_X 32.0f
#define GRID_SLOT_RIGHT_X 176.0f
#define GRID_TOP_MARGIN 240
static int s_rows = 0, s_cols = MAX_COL;
static int s_topIndex = 0; // 스크롤용
static Mix_Chunk *sfx_click = NULL;
static UIButton s_btnExit;
static UIButton s_btnPrevPage;
static UIButton s_btnNextPage;
static SDL_Texture *select_background = NULL;
static SDL_Texture *tex_exit = NULL;
static SDL_Texture *tex_prev = NULL;
static SDL_Texture *tex_next = NULL;
static SDL_Texture *start_grid = NULL;
static SDL_Texture *select_titlebar = NULL;
static int s_titlebarW = 0;
static int s_titlebarH = 0;
static void on_exit(void *ud)
{
    (void)ud;
    scene_switch(SCENE_MAINMENU);
}

static void choose_plant(int idx)
{
    G_SelectedPlantIndex = idx;
    SDL_Log("Selected plant idx=%d name=%s", idx, plantdb_get(idx)->name_kr);
    scene_switch_fade_arg(SCENE_PLANTINFO, (void *)(intptr_t)idx, 0.2f, 0.4f);
}

static void layout_ui(int w, int h)
{
    const int exitSize = 72;
    const int margin = 40;
    const int shadowInset = 6;
    s_btnExit.r = (SDL_Rect){margin, margin, exitSize, exitSize};

    const int navSize = exitSize;
    const int navGap = 24;
    int navY = h - margin - navSize - shadowInset;
    int navRight = w - margin - navSize - shadowInset;
    s_btnNextPage.r = (SDL_Rect){navRight, navY, navSize, navSize};
    s_btnPrevPage.r = (SDL_Rect){s_btnNextPage.r.x - navGap - navSize, navY, navSize, navSize};
}

static void change_page(int delta)
{
    int rows_visible = GRID_VISIBLE_ROWS;
    int maxTop = SDL_max(0, s_rows - rows_visible);
    s_topIndex += delta * rows_visible;
    if (s_topIndex < 0)
        s_topIndex = 0;
    if (s_topIndex > maxTop)
        s_topIndex = maxTop;
}

static void on_prev_page(void *ud)
{
    (void)ud;
    change_page(-1);
}

static void on_next_page(void *ud)
{
    (void)ud;
    change_page(1);
}

static SDL_Rect compute_grid_dest(int rendererW, int rendererH)
{
    float scale = 4.0f;
    float maxWidth = (float)rendererW - 160.0f;
    if (maxWidth < GRID_BASE_WIDTH)
        maxWidth = GRID_BASE_WIDTH;

    float maxHeight = (float)rendererH - 200.0f;
    if (maxHeight < GRID_BASE_HEIGHT)
        maxHeight = GRID_BASE_HEIGHT;

    float maxScaleW = maxWidth / GRID_BASE_WIDTH;
    if (scale > maxScaleW)
        scale = maxScaleW;

    float maxScaleH = maxHeight / GRID_BASE_HEIGHT;
    if (scale > maxScaleH)
        scale = maxScaleH;

    if (scale < 1.0f)
        scale = 1.0f;

    int scaledW = (int)(GRID_BASE_WIDTH * scale + 0.5f);
    int scaledH = (int)(GRID_BASE_HEIGHT * scale + 0.5f);

    int x = (rendererW - scaledW) / 2;
    if (x < 40)
        x = 40;
    if (x + scaledW > rendererW - 40)
    {
        x = rendererW - scaledW - 40;
        if (x < 40)
            x = 40;
    }

    int y = GRID_TOP_MARGIN;
    if (y + scaledH > rendererH - 50)
    {
        y = rendererH - scaledH - 50;
        if (y < 40)
            y = 40;
    }

    SDL_Rect dst = {x, y, scaledW, scaledH};
    return dst;
}

static SDL_Rect compute_slot_rect(int localRow, int col, const SDL_Rect *gridDst)
{
    float scaleX = gridDst->w / GRID_BASE_WIDTH;
    float scaleY = gridDst->h / GRID_BASE_HEIGHT;

    float baseX = (col == 0) ? GRID_SLOT_LEFT_X : GRID_SLOT_RIGHT_X;
    float baseY = GRID_ROW_START + (float)localRow * GRID_ROW_STEP;

    SDL_Rect rect;
    rect.x = gridDst->x + (int)(baseX * scaleX + 0.5f);
    rect.y = gridDst->y + (int)(baseY * scaleY + 0.5f);
    rect.w = (int)(GRID_SLOT_WIDTH * scaleX + 0.5f);
    rect.h = (int)(GRID_SLOT_HEIGHT * scaleY + 0.5f);
    return rect;
}

static void init(void *arg)
{
    (void)arg;
    if (!sfx_click)
        sfx_click = Mix_LoadWAV(ASSETS_SOUNDS_DIR "click.wav");
    // JSON 로드
    int n = plantdb_load(ASSETS_DIR "plants.json");
    if (n <= 0)
        SDL_Log("WARNING: plants.json empty or fail");

    int w, h;
    SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    ui_button_init(&s_btnExit, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&s_btnExit, on_exit, NULL);
    ui_button_set_sfx(&s_btnExit, sfx_click, NULL);

    ui_button_init(&s_btnPrevPage, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&s_btnPrevPage, on_prev_page, NULL);
    ui_button_set_sfx(&s_btnPrevPage, sfx_click, NULL);

    ui_button_init(&s_btnNextPage, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&s_btnNextPage, on_next_page, NULL);
    ui_button_set_sfx(&s_btnNextPage, sfx_click, NULL);

    s_rows = (n + s_cols - 1) / s_cols;
    s_topIndex = 0;

    layout_ui(w, h);

    if (!select_background)
    {
        select_background = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
        if (!select_background)
        {
            SDL_Log("Load select_Background.png failed: %s", IMG_GetError());
        }
    }

    if (!tex_exit)
    {
        tex_exit = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_exit.png");
        if (!tex_exit)
        {
            SDL_Log("Load I_exit.png failed: %s", IMG_GetError());
        }
    }

    if (!tex_prev)
    {
        tex_prev = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
        if (!tex_prev)
        {
            SDL_Log("Load I_back.png failed: %s", IMG_GetError());
        }
    }

    if (!tex_next)
    {
        tex_next = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_next.png");
        if (!tex_next)
        {
            SDL_Log("Load I_next.png failed: %s", IMG_GetError());
        }
    }

    if (!start_grid)
    {
        start_grid = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "start_grid.png");
        if (!start_grid)
        {
            SDL_Log("Load start_grid.png failed: %s", IMG_GetError());
        }
    }

    if (!select_titlebar)
    {
        select_titlebar = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_titlebar.png");
        if (!select_titlebar)
        {
            SDL_Log("Load select_titlebar.png failed: %s", IMG_GetError());
        }
        else
        {
            SDL_QueryTexture(select_titlebar, NULL, NULL, &s_titlebarW, &s_titlebarH);
        }
    }
}

static void handle(SDL_Event *e)
{
    if (e->type == SDL_QUIT)
    {
        G_Running = 0;
        return;
    }
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
    {
        layout_ui(e->window.data1, e->window.data2);
    }

    ui_button_handle(&s_btnExit, e);
    ui_button_handle(&s_btnPrevPage, e);
    ui_button_handle(&s_btnNextPage, e);

    // 마우스 클릭으로 항목 선택
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT)
    {
        int rw, rh;
        SDL_GetRendererOutputSize(G_Renderer, &rw, &rh);
        SDL_Rect gridDst = compute_grid_dest(rw, rh);
        int mx = e->button.x, my = e->button.y;
        for (int r = 0; r < GRID_VISIBLE_ROWS; r++)
        { // 화면에 보이는 최대 6행(가변이면 계산)
            for (int c = 0; c < s_cols; c++)
            {
                int idx = (s_topIndex + r) * s_cols + c;
                if (idx >= plantdb_count())
                    break;
                SDL_Rect cell = compute_slot_rect(r, c, &gridDst);
                if (ui_point_in_rect(mx, my, &cell))
                {
                    if (sfx_click)
                        Mix_PlayChannel(-1, sfx_click, 0);
                    choose_plant(idx);
                    return;
                }
            }
        }
    }

    // 휠 스크롤
    if (e->type == SDL_MOUSEWHEEL)
    {
        int rows_visible = GRID_VISIBLE_ROWS;
        int maxTop = SDL_max(0, s_rows - rows_visible);
        s_topIndex -= (e->wheel.y > 0) ? 1 : (e->wheel.y < 0 ? -1 : 0);
        if (s_topIndex < 0)
            s_topIndex = 0;
        if (s_topIndex > maxTop)
            s_topIndex = maxTop;
    }
}

static void update(float dt) { (void)dt; }

static void render(SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);

    if (select_background)
    {
        int w, h;
        SDL_GetRendererOutputSize(r, &w, &h);
        SDL_Rect dst = {0, 0, w, h};
        SDL_RenderCopy(r, select_background, NULL, &dst);
    }
    else
    {
        int w, h;
        SDL_GetRendererOutputSize(r, &w, &h);
        for (int y = 0; y < h; ++y)
        {
            float t = (float)y / (float)h;
            Uint8 R = (Uint8)(16 + t * 20);
            Uint8 G = (Uint8)(20 + t * 60);
            Uint8 B = (Uint8)(28 + t * 40);
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }
    // 타이틀
    if (G_FontMain)
    {
        SDL_Color c = {230, 240, 235, 255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(G_FontMain, "키울 식물을 선택하세요", c);
        if (s)
        {
            SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
            int w, h;
            SDL_GetRendererOutputSize(r, &w, &h);
            int tw, th;
            SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            int titleCenterY = 140;
            float titleScale = 1.5f;
            int scaledW = (int)(tw * titleScale + 0.5f);
            int scaledH = (int)(th * titleScale + 0.5f);
            if (scaledW < 1)
                scaledW = 1;
            if (scaledH < 1)
                scaledH = 1;

            if (select_titlebar && s_titlebarW > 0 && s_titlebarH > 0)
            {
                int desiredWidth = scaledW + 100;
                int barScale = (desiredWidth + s_titlebarW - 1) / s_titlebarW;
                if (barScale < 1)
                    barScale = 1;
                int barW = s_titlebarW * barScale;
                int barH = s_titlebarH * barScale;
                SDL_Rect barRect = {(w - barW) / 2, titleCenterY - barH / 2, barW, barH};
                SDL_RenderCopy(r, select_titlebar, NULL, &barRect);
            }

            SDL_Rect d = {(w - scaledW) / 2, titleCenterY - scaledH / 2, scaledW, scaledH};
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    // 그리드
    int rw, rh;
    SDL_GetRendererOutputSize(r, &rw, &rh);
    SDL_Rect gridDst = compute_grid_dest(rw, rh);
    if (start_grid)
    {
        SDL_RenderCopy(r, start_grid, NULL, &gridDst);
    }

    for (int rrow = 0; rrow < GRID_VISIBLE_ROWS; rrow++)
    {
        for (int c = 0; c < s_cols; c++)
        {
            int idx = (s_topIndex + rrow) * s_cols + c;
            if (idx >= plantdb_count())
                break;

            const PlantInfo *p = plantdb_get(idx);
            SDL_Rect cell = compute_slot_rect(rrow, c, &gridDst);

            if (G_FontMain && p)
            {
                float scaleX = (float)cell.w / GRID_SLOT_WIDTH;
                int paddingX = (int)(12 * scaleX);
                if (paddingX < 8)
                    paddingX = 8;

                SDL_Color nameColor = {60, 36, 24, 255};
                SDL_Surface *sName = TTF_RenderUTF8_Blended(G_FontMain, p->name_kr, nameColor);
                if (sName)
                {
                    SDL_Texture *texName = SDL_CreateTextureFromSurface(r, sName);
                    int tw, th;
                    SDL_QueryTexture(texName, NULL, NULL, &tw, &th);
                    int availableWidth = cell.w - paddingX * 2;
                    if (availableWidth < 1)
                        availableWidth = cell.w - 2;
                    if (availableWidth < 1)
                        availableWidth = 1;
                    if (tw > availableWidth)
                    {
                        double shrink = (double)availableWidth / (double)tw;
                        tw = availableWidth;
                        th = (int)(th * shrink);
                    }
                    if (tw < 1)
                        tw = 1;
                    if (th < 1)
                        th = 1;
                    int nameY = cell.y + (cell.h - th) / 2;
                    SDL_Rect nameDst = {cell.x + paddingX, nameY, tw, th};
                    SDL_RenderCopy(r, texName, NULL, &nameDst);
                    SDL_DestroyTexture(texName);
                    SDL_FreeSurface(sName);
                }
            }
        }
    }

    // 뒤로 버튼
    ui_button_render(r, G_FontMain, &s_btnExit, tex_exit);
    ui_button_render(r, G_FontMain, &s_btnPrevPage, tex_prev);
    ui_button_render(r, G_FontMain, &s_btnNextPage, tex_next);
}

static void cleanup(void)
{
    if (sfx_click)
    {
        Mix_FreeChunk(sfx_click);
        sfx_click = NULL;
    }
    if (select_titlebar)
    {
        SDL_DestroyTexture(select_titlebar);
        select_titlebar = NULL;
        s_titlebarW = 0;
        s_titlebarH = 0;
    }
    if (start_grid)
    {
        SDL_DestroyTexture(start_grid);
        start_grid = NULL;
    }
    if (tex_prev)
    {
        SDL_DestroyTexture(tex_prev);
        tex_prev = NULL;
    }
    if (tex_next)
    {
        SDL_DestroyTexture(tex_next);
        tex_next = NULL;
    }
    if (tex_exit)
    {
        SDL_DestroyTexture(tex_exit);
        tex_exit = NULL;
    }
    if (select_background)
    {
        SDL_DestroyTexture(select_background);
        select_background = NULL;
    }
    // plantdb_free();  // 선택 유지하려면 여기서 즉시 free하지 말고, 프로그램 종료 때 정리
}

static Scene SCENE_OBJ = {init, handle, update, render, cleanup, "SelectPlant"};
Scene *scene_selectplant_object(void) { return &SCENE_OBJ; }
