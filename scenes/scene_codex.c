#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/core.h"
#include "../include/save.h"
#include "../include/ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>

// -- 외부 훅
extern int plantdb_count(void);
extern const PlantInfo *plantdb_get(int idx);
// 추후 save , stats 에 맞춰서 구현
bool save_is_plant_completed(const char *plant_id);
int save_get_plant_logs(const char *plant_id, CodexLog *out, int max);

// --- 내부 ----
typedef struct
{
    int unlocked;
    SDL_Texture *thumb; // 해금 썸네일
} CodexEntry;

static CodexEntry *s_entries = NULL;
static int s_count = 0;

static SDL_Texture *s_locked = NULL;
static SDL_Texture *s_bg = NULL;
static SDL_Texture *s_backIcon = NULL;
static SDL_Texture *s_nextIcon = NULL;
static SDL_Texture *s_titlebar = NULL;
static int s_titlebarW = 0;
static int s_titlebarH = 0;

static UIButton s_btnBack;
static UIButton s_btnPrevPage;
static UIButton s_btnNextPage;
static SDL_Rect s_gridBoard = {0};
static SDL_Rect s_gridInner = {0};
static SDL_Rect s_detailBoard = {0};
static int s_visibleRows = 1;
static int s_itemsPerPage = 0;
static int s_page = 0;

static int s_selected = -1;
static TTF_Font *s_font = NULL;
static TTF_Font *s_titleFont = NULL;

static void clamp_page(void);

// 그리드 레이아웃
enum
{
    CELL = 128,
    GAP = 18,
    COLS = 5
};
static int grid_origin_x = 56;
static int grid_origin_y = 130;

// 유틸리티
static SDL_Texture *load_tex(const char *path)
{
    SDL_Texture *t = IMG_LoadTexture(G_Renderer, path);
    if (!t)
        SDL_Log("Codex: IMG load fail %s : %s", path, IMG_GetError());
    return t;
}

static void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *s, SDL_Color c)
{
    if (!f || !s)
        return;
    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, s, c);
    if (!sf)
        return;
    SDL_Texture *tx = SDL_CreateTextureFromSurface(r, sf);
    SDL_Rect d = {x, y, sf->w, sf->h};
    SDL_FreeSurface(sf);
    SDL_RenderCopy(r, tx, NULL, &d);
    SDL_DestroyTexture(tx);
}

static void on_back(void *ud)
{
    (void)ud;
    scene_switch(SCENE_MAINMENU);
}

static void change_page(int delta)
{
    int previous = s_page;
    s_page += delta;
    clamp_page();
    if (s_page != previous)
    {
        int start = s_page * s_itemsPerPage;
        int end = start + s_itemsPerPage;
        if (s_selected < start || s_selected >= end)
            s_selected = -1;
    }
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

static void clamp_page(void)
{
    if (s_itemsPerPage <= 0)
        s_itemsPerPage = COLS;
    int totalPages = (s_count + s_itemsPerPage - 1) / s_itemsPerPage;
    if (totalPages <= 0)
        totalPages = 1;
    if (s_page < 0)
        s_page = 0;
    if (s_page >= totalPages)
        s_page = totalPages - 1;

    s_btnPrevPage.enabled = (totalPages > 1 && s_page > 0);
    s_btnNextPage.enabled = (totalPages > 1 && s_page < totalPages - 1);
}

static void layout(int win_w, int win_h)
{
    const int margin = 40;
    const int top = 160;
    const int gap = 48;

    int detailWidth = SDL_max(360, win_w / 4);
    int boardHeight = win_h - top - margin;
    if (boardHeight < 320)
        boardHeight = 320;

    int gridWidth = win_w - (margin * 2 + gap + detailWidth);
    if (gridWidth < 420)
        gridWidth = 420;
    if (gridWidth + detailWidth + margin * 2 + gap > win_w)
    {
        detailWidth = SDL_max(320, win_w - (margin * 2 + gap + gridWidth));
        if (detailWidth < 320)
            detailWidth = 320;
    }

    s_gridBoard = (SDL_Rect){margin, top, gridWidth, boardHeight};
    s_detailBoard = (SDL_Rect){s_gridBoard.x + s_gridBoard.w + gap, top, detailWidth, boardHeight};
    if (s_detailBoard.x + s_detailBoard.w > win_w - margin)
        s_detailBoard.x = win_w - margin - s_detailBoard.w;

    s_gridInner = (SDL_Rect){s_gridBoard.x + 24, s_gridBoard.y + 24, s_gridBoard.w - 48, s_gridBoard.h - 48};

    s_btnBack.r = (SDL_Rect){margin, margin, 72, 72};

    grid_origin_x = s_gridInner.x + 8;
    grid_origin_y = s_gridInner.y + 8;

    int usableHeight = s_gridInner.h - 32;
    if (usableHeight < CELL)
        usableHeight = CELL;
    s_visibleRows = usableHeight / (CELL + GAP);
    if (s_visibleRows < 1)
        s_visibleRows = 1;

    s_itemsPerPage = s_visibleRows * COLS;
    if (s_itemsPerPage < COLS)
        s_itemsPerPage = COLS;

    int navSize = 72;
    int navGap = 20;
    int navY = s_gridBoard.y + s_gridBoard.h - navSize - 20;
    if (navY < s_gridBoard.y + 20)
        navY = s_gridBoard.y + 20;
    s_btnNextPage.r = (SDL_Rect){s_gridBoard.x + s_gridBoard.w - navSize - 24, navY, navSize, navSize};
    s_btnPrevPage.r = (SDL_Rect){s_btnNextPage.r.x - navGap - navSize, navY, navSize, navSize};

    clamp_page();
}

// 초기화
static void init(void *arg)
{
    (void)arg;
    int w, h;
    SDL_GetRendererOutputSize(G_Renderer, &w, &h);

    ui_button_init(&s_btnBack, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&s_btnBack, on_back, NULL);
    ui_button_set_sfx(&s_btnBack, G_SFX_Click, G_SFX_Hover);

    ui_button_init(&s_btnPrevPage, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&s_btnPrevPage, on_prev_page, NULL);
    ui_button_set_sfx(&s_btnPrevPage, G_SFX_Click, G_SFX_Hover);

    ui_button_init(&s_btnNextPage, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&s_btnNextPage, on_next_page, NULL);
    ui_button_set_sfx(&s_btnNextPage, G_SFX_Click, G_SFX_Hover);

    layout(w, h);

    if (!s_font)
    {
    s_font = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 24);
    if (!s_font)
        SDL_Log("CODEX 폰트로드 실패:%s", TTF_GetError());
    }

    if (!s_titleFont)
    {
    s_titleFont = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 40);
    if (!s_titleFont)
        SDL_Log("CODEX 타이틀 폰트로드 실패:%s", TTF_GetError());
    }

    if (!s_bg)
    {
    s_bg = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
    if (!s_bg)
        SDL_Log("CODEX 배경 로드 실패: %s", IMG_GetError());
    }

    if (!s_backIcon)
    {
        s_backIcon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
        if (!s_backIcon)
            SDL_Log("CODEX 뒤로 아이콘 로드 실패: %s", IMG_GetError());
    }

    if (!s_nextIcon)
    {
        s_nextIcon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_next.png");
        if (!s_nextIcon)
            SDL_Log("CODEX 다음 아이콘 로드 실패: %s", IMG_GetError());
    }

    if (!s_titlebar)
    {
        s_titlebar = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_titlebar.png");
        if (!s_titlebar)
            SDL_Log("CODEX 타이틀바 로드 실패: %s", IMG_GetError());
        else
            SDL_QueryTexture(s_titlebar, NULL, NULL, &s_titlebarW, &s_titlebarH);
    }

    // 데이터 수집
    s_count = plantdb_count();
    s_entries = (CodexEntry *)SDL_calloc(s_count, sizeof(CodexEntry));
    if (!s_entries)
    {
        SDL_Log("CODEX: alloc fail");
        return;
    }

    // 공용잠금 텍스처
    s_locked = load_tex(ASSETS_IMAGES_DIR "codex/locked_collection.png");

    // 각 식물별 썸네일 로드
    for (int i = 0; i < s_count; i++)
    {
        const PlantInfo *p = plantdb_get(i);
        if (!p)
            continue;
        s_entries[i].unlocked = save_is_plant_completed(p->id) ? 1 : 0;

        if (s_entries[i].unlocked)
        {
            char path[512];
            SDL_snprintf(path, sizeof(path), ASSETS_IMAGES_DIR "codex/%s_thumb.png", p->id);
            s_entries[i].thumb = load_tex(path);
        }
        else
        {
            s_entries[i].thumb = NULL;
        }
    }

    s_page = 0;
    s_selected = -1;
    clamp_page();
}

// 이벤트 처리
static void handle(SDL_Event *e)
{
    if (e->type == SDL_QUIT)
    {
        G_Running = 0;
        return;
    }

    ui_button_handle(&s_btnBack, e);
    ui_button_handle(&s_btnPrevPage, e);
    ui_button_handle(&s_btnNextPage, e);

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
    {
        layout(e->window.data1, e->window.data2);
        return;
    }

    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE)
    {
        scene_switch(SCENE_MAINMENU);
        return;
    }

    if (e->type == SDL_MOUSEWHEEL)
    {
        if (e->wheel.y > 0)
            change_page(-1);
        else if (e->wheel.y < 0)
            change_page(1);
        return;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
    {
        int mx = e->button.x, my = e->button.y;
        if (!(mx >= s_gridInner.x && mx < s_gridInner.x + s_gridInner.w && my >= s_gridInner.y && my < s_gridInner.y + s_gridInner.h))
            return;

        int startIndex = s_page * s_itemsPerPage;
        for (int row = 0; row < s_visibleRows; ++row)
        {
            for (int col = 0; col < COLS; ++col)
            {
                int idx = startIndex + row * COLS + col;
                if (idx >= s_count)
                    return;
                SDL_Rect rc = {grid_origin_x + col * (CELL + GAP), grid_origin_y + row * (CELL + GAP), CELL, CELL};
                if (ui_point_in_rect(mx, my, &rc))
                {
                    s_selected = idx;
                    return;
                }
            }
        }
    }
}
static void update(float dt)
{
    (void)dt;
}

static void render(SDL_Renderer *r)
{
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);

    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    SDL_RenderClear(r);

    if (s_bg)
    {
    SDL_Rect dst = {0, 0, w, h};
    SDL_RenderCopy(r, s_bg, NULL, &dst);
    }
    else
    {
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
    if (s_titleFont)
    {
    const char *title = "도감";
    SDL_Surface *titleSurf = TTF_RenderUTF8_Blended(s_titleFont, title, (SDL_Color){245, 245, 240, 255});
    if (titleSurf)
    {
        SDL_Texture *titleTex = SDL_CreateTextureFromSurface(r, titleSurf);
        int tw = titleSurf->w;
        int th = titleSurf->h;
        int scaledW = (int)(tw * 1.4f + 0.5f);
        int scaledH = (int)(th * 1.4f + 0.5f);
        if (scaledW < 1)
            scaledW = 1;
        if (scaledH < 1)
            scaledH = 1;
        int centerX = s_gridBoard.x + s_gridBoard.w / 2;
        int titleY = 110;

        if (s_titlebar && s_titlebarW > 0 && s_titlebarH > 0)
        {
            int desiredWidth = scaledW + 220;
            int barScale = (desiredWidth + s_titlebarW - 1) / s_titlebarW;
            if (barScale < 1)
                barScale = 1;
            SDL_Rect barDst = {centerX - (s_titlebarW * barScale) / 2, titleY - (s_titlebarH * barScale) / 2, s_titlebarW * barScale, s_titlebarH * barScale};
            SDL_RenderCopy(r, s_titlebar, NULL, &barDst);
        }

        SDL_Rect titleDst = {centerX - scaledW / 2, titleY - scaledH / 2, scaledW, scaledH};
        SDL_RenderCopy(r, titleTex, NULL, &titleDst);
        SDL_DestroyTexture(titleTex);
        SDL_FreeSurface(titleSurf);
    }
    }

    // 그리드 보드
    SDL_SetRenderDrawColor(r, 179, 139, 98, 255);
    SDL_RenderFillRect(r, &s_gridBoard);
    SDL_SetRenderDrawColor(r, 92, 63, 44, 255);
    SDL_RenderDrawRect(r, &s_gridBoard);

    SDL_SetRenderDrawColor(r, 236, 216, 188, 255);
    SDL_RenderFillRect(r, &s_gridInner);
    SDL_SetRenderDrawColor(r, 141, 101, 66, 160);
    SDL_RenderDrawRect(r, &s_gridInner);

    int startIndex = s_page * s_itemsPerPage;
    int x0 = grid_origin_x;
    int yBase = grid_origin_y;
    for (int row = 0; row < s_visibleRows; ++row)
    {
        for (int col = 0; col < COLS; ++col)
        {
            int idx = startIndex + row * COLS + col;
            SDL_Rect cell = {x0 + col * (CELL + GAP), yBase + row * (CELL + GAP), CELL, CELL};

            SDL_Rect cellFrame = cell;
            SDL_SetRenderDrawColor(r, 214, 187, 145, 255);
            SDL_RenderFillRect(r, &cellFrame);
            SDL_SetRenderDrawColor(r, 115, 78, 52, 255);
            SDL_RenderDrawRect(r, &cellFrame);

            if (idx < s_count)
            {
                SDL_Texture *tex = s_entries[idx].unlocked ? s_entries[idx].thumb : s_locked;
                if (tex)
                {
                    int tw, th;
                    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
                    float scale = (float)(CELL - 20) / (float)((tw > th) ? tw : th);
                    if (scale > 1.f)
                        scale = 1.f;
                    int rw = (int)(tw * scale);
                    int rh = (int)(th * scale);
                    SDL_Rect dst = {cell.x + (CELL - rw) / 2, cell.y + (CELL - rh) / 2, rw, rh};
                    SDL_RenderCopy(r, tex, NULL, &dst);
                }

                if (s_entries[idx].unlocked && s_font)
                {
                    const PlantInfo *p = plantdb_get(idx);
                    if (p)
                    {
                        SDL_Color c = {245, 245, 240, 255};
                        draw_text(r, s_font, cell.x, cell.y + CELL + 6, p->name_kr, c);
                    }
                }

                if (idx == s_selected)
                {
                    SDL_SetRenderDrawColor(r, 255, 230, 120, 200);
                    SDL_RenderDrawRect(r, &cell);
                }
            }
        }
    }
    // 디테일 보드
    SDL_SetRenderDrawColor(r, 179, 139, 98, 255);
    SDL_RenderFillRect(r, &s_detailBoard);
    SDL_SetRenderDrawColor(r, 92, 63, 44, 255);
    SDL_RenderDrawRect(r, &s_detailBoard);

    SDL_Rect detailInner = {s_detailBoard.x + 24, s_detailBoard.y + 24, s_detailBoard.w - 48, s_detailBoard.h - 48};
    SDL_SetRenderDrawColor(r, 244, 234, 214, 255);
    SDL_RenderFillRect(r, &detailInner);
    SDL_SetRenderDrawColor(r, 141, 101, 66, 160);
    SDL_RenderDrawRect(r, &detailInner);

    if (s_selected >= 0 && s_selected < s_count)
    {
    const PlantInfo *p = plantdb_get(s_selected);
    if (p)
    {
        int textX = detailInner.x + 20;
        int cursorY = detailInner.y + 20;
        SDL_Color nameColor = {245, 245, 240, 255};
        SDL_Color bodyColor = {230, 224, 210, 255};
        SDL_Color subtle = {212, 196, 176, 255};

        if (s_titleFont)
        {
            SDL_Surface *nameSurf = TTF_RenderUTF8_Blended(s_titleFont, p->name_kr, nameColor);
            if (nameSurf)
            {
                SDL_Texture *nameTex = SDL_CreateTextureFromSurface(r, nameSurf);
                int scaledW = (int)(nameSurf->w * 0.9f);
                int scaledH = (int)(nameSurf->h * 0.9f);
                SDL_Rect dst = {textX, cursorY, scaledW, scaledH};
                SDL_RenderCopy(r, nameTex, NULL, &dst);
                cursorY += scaledH + 12;
                SDL_DestroyTexture(nameTex);
                SDL_FreeSurface(nameSurf);
            }
        }

        char latinLine[128];
        SDL_snprintf(latinLine, sizeof(latinLine), "학명: %s", p->latin);
        draw_text(r, s_font, textX, cursorY, latinLine, subtle);
        cursorY += 36;

        if (s_entries[s_selected].unlocked)
        {
            char path[512];
            SDL_snprintf(path, sizeof(path), ASSETS_IMAGES_DIR "codex/%s_ful.png", p->id);
            SDL_Texture *full = load_tex(path);
            if (full)
            {
                int tw, th;
                SDL_QueryTexture(full, NULL, NULL, &tw, &th);
                int maxW = detailInner.w - 40;
                int maxH = 200;
                float s = (float)maxW / (float)tw;
                if (th * s > maxH)
                    s = (float)maxH / (float)th;
                if (s > 1.f)
                    s = 1.f;
                int rw = (int)(tw * s);
                int rh = (int)(th * s);
                SDL_Rect d = {textX, cursorY, rw, rh};
                SDL_RenderCopy(r, full, NULL, &d);
                cursorY += rh + 16;
                SDL_DestroyTexture(full);
            }
        }
        else
        {
            draw_text(r, s_font, textX, cursorY, "미해금 - 꽃을 피워보세요", (SDL_Color){220, 120, 120, 255});
            cursorY += 40;
        }

        if (s_entries[s_selected].unlocked)
        {
            char info1[128];
            SDL_snprintf(info1, sizeof(info1), "물 주기: %d일 간격", p->water_days);
            char info2[128];
            const char *lightLevels[] = {"약", "중간", "강"};
			int lightIdx = p->light_level;
			if (lightIdx < 0)
				lightIdx = 0;
			if (lightIdx > 2)
				lightIdx = 2;
            SDL_snprintf(info2, sizeof(info2), "햇빛: %s", lightLevels[lightIdx]);
            char info3[128];
            SDL_snprintf(info3, sizeof(info3), "적정 온도: %d℃ ~ %d℃", p->min_temp, p->max_temp);
            draw_text(r, s_font, textX, cursorY, info1, bodyColor);
            cursorY += 26;
            draw_text(r, s_font, textX, cursorY, info2, bodyColor);
            cursorY += 26;
            draw_text(r, s_font, textX, cursorY, info3, bodyColor);
            cursorY += 40;
        }

        if (s_entries[s_selected].unlocked)
        {
            CodexLog logs[64];
            int n = save_get_plant_logs(p->id, logs, 64);
            draw_text(r, s_font, textX, cursorY, "기록", bodyColor);
            cursorY += 30;
            for (int i = 0; i < n && cursorY < detailInner.y + detailInner.h - 24; ++i)
            {
                draw_text(r, s_font, textX, cursorY, logs[i].line, subtle);
                cursorY += 24;
            }
        }
    }
    }

    // 버튼
    ui_button_render(r, NULL, &s_btnBack, s_backIcon);
    ui_button_render(r, NULL, &s_btnPrevPage, s_backIcon);
    ui_button_render(r, NULL, &s_btnNextPage, s_nextIcon);
}

static void cleanup(void)
{
    if (s_entries)
    {
    for (int i = 0; i < s_count; i++)
    {
        if (s_entries[i].thumb)
            SDL_DestroyTexture(s_entries[i].thumb);
    }
    SDL_free(s_entries);
    s_entries = NULL;
    }

    if (s_locked)
    {
    SDL_DestroyTexture(s_locked);
    s_locked = NULL;
    }

    if (s_backIcon)
    {
        SDL_DestroyTexture(s_backIcon);
        s_backIcon = NULL;
    }
    if (s_nextIcon)
    {
        SDL_DestroyTexture(s_nextIcon);
        s_nextIcon = NULL;
    }

    if (s_titlebar)
    {
    SDL_DestroyTexture(s_titlebar);
    s_titlebar = NULL;
    s_titlebarW = 0;
    s_titlebarH = 0;
    }

    if (s_bg)
    {
    SDL_DestroyTexture(s_bg);
    s_bg = NULL;
    }

    if (s_font)
    {
    TTF_CloseFont(s_font);
    s_font = NULL;
    }

    if (s_titleFont)
    {
        TTF_CloseFont(s_titleFont);
        s_titleFont = NULL;
    }

    s_page = 0;
    s_selected = -1;
}

// 씬 객체
static Scene SCENE_OBJ = {init, handle, update, render, cleanup, "Codex"};
Scene *scene_codex_object(void)
{
    return &SCENE_OBJ;
}
