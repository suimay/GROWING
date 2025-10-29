#include "../include/scene_plantinfo.h"
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/core.h"
#include "../include/ui.h"
#include "../include/settings.h"
#include "../include/loading.h"
#include "../include/gameplay.h" 
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <stdarg.h>


typedef struct
{
    int plant_index;
    const PlantInfo *plant;

    SDL_Texture *background;
    SDL_Texture *titlebar;
    SDL_Texture *plant_texture;
    SDL_Texture *button_texture;
    SDL_Texture *back_icon;

    int titlebar_w;
    int titlebar_h;

    SDL_Rect panel_rect;

    UIButton btn_back;
    UIButton btn_select;

    TTF_Font *font_title;
    TTF_Font *font_body;
    TTF_Font *font_small;
} PlantInfoCtx;


static PlantInfoCtx s_ctx = {0};


static void plantinfo_on_back(void *ud);
static void plantinfo_on_select(void *ud);
static void layout(int w, int h);

extern int gameplay_loading_job(void* userdata, float* out_progress);

static void draw_text(SDL_Renderer *r, TTF_Font *font, SDL_Color color, int x, int y, const char *fmt, ...)
{
    if (!font || !fmt)
        return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, buf, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static bool ensure_resources_loaded(void)
{
    if (!s_ctx.background)
    {
        s_ctx.background = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
        if (!s_ctx.background)
            SDL_Log("PLANTINFO: failed to load select_Background.png: %s", IMG_GetError());
    }
    if (!s_ctx.titlebar)
    {
        s_ctx.titlebar = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_titlebar.png");
        if (!s_ctx.titlebar)
            SDL_Log("PLANTINFO: failed to load select_titlebar.png: %s", IMG_GetError());
        else
            SDL_QueryTexture(s_ctx.titlebar, NULL, NULL, &s_ctx.titlebar_w, &s_ctx.titlebar_h);
    }
    if (!s_ctx.button_texture)
    {
        s_ctx.button_texture = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_basic.png");
        if (!s_ctx.button_texture)
            SDL_Log("PLANTINFO: failed to load B_basic.png: %s", IMG_GetError());
    }
    if (!s_ctx.back_icon)
    {
        s_ctx.back_icon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
        if (!s_ctx.back_icon)
            SDL_Log("PLANTINFO: failed to load I_back.png: %s", IMG_GetError());
    }
    if (!s_ctx.font_title)
    {
        s_ctx.font_title = TTF_OpenFont(ASSETS_FONTS_DIR "NeoDunggeunmoPro-Regular.ttf", 44);
        if (!s_ctx.font_title)
            SDL_Log("PLANTINFO: title font load failed: %s", TTF_GetError());
    }
    if (!s_ctx.font_body)
    {
        s_ctx.font_body = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 30);
        if (!s_ctx.font_body)
            SDL_Log("PLANTINFO: body font load failed: %s", TTF_GetError());
    }
    if (!s_ctx.font_small)
    {
        s_ctx.font_small = TTF_OpenFont(ASSETS_FONTS_DIR "NeoDunggeunmoPro-Regular.ttf", 24);
        if (!s_ctx.font_small)
            SDL_Log("PLANTINFO: small font load failed: %s", TTF_GetError());
    }
    return s_ctx.background && s_ctx.titlebar && s_ctx.button_texture && s_ctx.back_icon &&
           s_ctx.font_title && s_ctx.font_body && s_ctx.font_small;
}

static const char *light_text(int level)
{
    static const char *names[] = {"약", "중간", "강"};
    if (level < 0)
        level = 0;
    if (level > 2)
        level = 2;
    return names[level];
}

static void layout(int w, int h)
{
    const int margin = 60;
    int panelW = w - margin * 2;
    if (panelW > 1100)
        panelW = 1100;
    int panelH = h - (margin * 2 + 80);
    if (panelH < 520)
        panelH = 520;

    s_ctx.panel_rect = (SDL_Rect){(w - panelW) / 2, margin + 50, panelW, panelH};

    ui_button_init(&s_ctx.btn_back, (SDL_Rect){40, 40, 72, 72}, "");
    ui_button_set_callback(&s_ctx.btn_back, plantinfo_on_back, NULL);
    ui_button_set_sfx(&s_ctx.btn_back, G_SFX_Click, G_SFX_Hover);

    int selectW = 280;
    int selectH = 64;
    int selectX = s_ctx.panel_rect.x + (s_ctx.panel_rect.w - selectW) / 2;
    int selectY = s_ctx.panel_rect.y + s_ctx.panel_rect.h - selectH - 30;
    ui_button_init(&s_ctx.btn_select, (SDL_Rect){selectX, selectY, selectW, selectH}, "이 식물 선택");
    ui_button_set_callback(&s_ctx.btn_select, plantinfo_on_select, NULL);
    ui_button_set_sfx(&s_ctx.btn_select, G_SFX_Click, G_SFX_Hover);
}

static void destroy_plant_texture(void)
{
    if (s_ctx.plant_texture)
    {
        SDL_DestroyTexture(s_ctx.plant_texture);
        s_ctx.plant_texture = NULL;
    }
}

bool plantinfo_init(void *arg)
{
    int idx = -1;
    if (arg)
        idx = (int)(intptr_t)arg;
    else if (G_SelectedPlantIndex >= 0)
        idx = G_SelectedPlantIndex;

    if (idx < 0)
        return false;

    s_ctx.plant_index = idx;
    s_ctx.plant = plantdb_get(idx);
    G_SelectedPlantIndex = idx;
    if (!s_ctx.plant)
        return false;

    if (!ensure_resources_loaded())
        return false;

    destroy_plant_texture();
    if (s_ctx.plant->icon_path[0])
    {
        s_ctx.plant_texture = IMG_LoadTexture(G_Renderer, s_ctx.plant->icon_path);
        if (!s_ctx.plant_texture)
            SDL_Log("PLANTINFO: failed to load plant icon %s: %s", s_ctx.plant->icon_path, IMG_GetError());
    }

    int w, h;
    SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    layout(w, h);

    return true;
}

void plantinfo_handle_event(const SDL_Event *e)
{
    if (e->type == SDL_QUIT)
    {
        G_Running = 0;
        return;
    }

    ui_button_handle(&s_ctx.btn_back, e);
    ui_button_handle(&s_ctx.btn_select, e);

    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE)
    {
        scene_switch(SCENE_SELECT_PLANT);
        return;
    }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
    {
        layout(e->window.data1, e->window.data2);
    }
}

void plantinfo_update(float dt)
{
    (void)dt;
}

void plantinfo_render(void)
{
    SDL_Renderer *r = G_Renderer;
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);

    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
    SDL_RenderClear(r);

    if (s_ctx.background)
    {
        SDL_Rect dst = {0, 0, w, h};
        SDL_RenderCopy(r, s_ctx.background, NULL, &dst);
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

    SDL_SetRenderDrawColor(r, 179, 139, 98, 255);
    SDL_RenderFillRect(r, &s_ctx.panel_rect);
    SDL_SetRenderDrawColor(r, 92, 63, 44, 255);
    SDL_RenderDrawRect(r, &s_ctx.panel_rect);

    SDL_Rect inner = {s_ctx.panel_rect.x + 24, s_ctx.panel_rect.y + 24, s_ctx.panel_rect.w - 48, s_ctx.panel_rect.h - 48};
    SDL_SetRenderDrawColor(r, 236, 216, 188, 255);
    SDL_RenderFillRect(r, &inner);
    SDL_SetRenderDrawColor(r, 141, 101, 66, 160);
    SDL_RenderDrawRect(r, &inner);

    if (s_ctx.titlebar && s_ctx.titlebar_w > 0 && s_ctx.titlebar_h > 0)
    {
        int centerX = s_ctx.panel_rect.x + s_ctx.panel_rect.w / 2;
        int titleY = s_ctx.panel_rect.y - 40;
        int desiredWidth = (int)(s_ctx.panel_rect.w * 0.6f);
        if (desiredWidth < 360)
            desiredWidth = 360;
        int scale = (desiredWidth + s_ctx.titlebar_w - 1) / s_ctx.titlebar_w;
        if (scale < 1)
            scale = 1;
        SDL_Rect barDst = {centerX - (s_ctx.titlebar_w * scale) / 2, titleY - (s_ctx.titlebar_h * scale) / 2, s_ctx.titlebar_w * scale, s_ctx.titlebar_h * scale};
        SDL_RenderCopy(r, s_ctx.titlebar, NULL, &barDst);
    }

    if (s_ctx.plant)
    {
        int iconAreaWidth = inner.w / 3;
        SDL_Rect iconArea = {inner.x + 20, inner.y + 20, iconAreaWidth - 40, inner.h - 160};
        SDL_SetRenderDrawColor(r, 206, 188, 158, 255);
        SDL_RenderFillRect(r, &iconArea);
        SDL_SetRenderDrawColor(r, 138, 104, 70, 180);
        SDL_RenderDrawRect(r, &iconArea);

        if (s_ctx.plant_texture)
        {
            int texW, texH;
            if (SDL_QueryTexture(s_ctx.plant_texture, NULL, NULL, &texW, &texH) == 0 && texW > 0 && texH > 0)
            {
                float scale = (float)(iconArea.w - 40) / (float)texW;
                if ((int)(texH * scale) > iconArea.h - 40)
                    scale = (float)(iconArea.h - 40) / (float)texH;
                if (scale > 1.f)
                    scale = 1.f;
                int drawW = (int)(texW * scale);
                int drawH = (int)(texH * scale);
                SDL_Rect dst = {
                    iconArea.x + (iconArea.w - drawW) / 2,
                    iconArea.y + (iconArea.h - drawH) / 2,
                    drawW,
                    drawH};
                SDL_RenderCopy(r, s_ctx.plant_texture, NULL, &dst);
            }
        }

        int textX = inner.x + iconAreaWidth + 20;
        int textY = inner.y + 20;
        SDL_Color nameColor = {64, 40, 22, 255};
        SDL_Color bodyColor = {82, 60, 42, 255};
        SDL_Color subtleColor = {118, 96, 74, 255};

        draw_text(r, s_ctx.font_title, nameColor, textX, textY, "%s", s_ctx.plant->name_kr);
        textY += 48;
        draw_text(r, s_ctx.font_body, subtleColor, textX, textY, "학명: %s", s_ctx.plant->latin);
        textY += 40;

        draw_text(r, s_ctx.font_body, bodyColor, textX, textY, "물 주기: %d일 간격", s_ctx.plant->water_days);
        textY += 36;
        draw_text(r, s_ctx.font_body, bodyColor, textX, textY, "빛: %s", light_text(s_ctx.plant->light_level));
        textY += 36;
        draw_text(r, s_ctx.font_body, bodyColor, textX, textY, "온도: %d ℃ ~ %d ℃", s_ctx.plant->min_temp, s_ctx.plant->max_temp);
        textY += 48;
        draw_text(r, s_ctx.font_small, subtleColor, textX, textY, "Tip: 이 식물의 환경을 확인하고 준비해 주세요!");
    }

    ui_button_render(r, NULL, &s_ctx.btn_back, s_ctx.back_icon);
    ui_button_render(r, s_ctx.font_body, &s_ctx.btn_select, s_ctx.button_texture);
}

void plantinfo_cleanup(void)
{
    destroy_plant_texture();

    if (s_ctx.background)
    {
        SDL_DestroyTexture(s_ctx.background);
        s_ctx.background = NULL;
    }
    if (s_ctx.titlebar)
    {
        SDL_DestroyTexture(s_ctx.titlebar);
        s_ctx.titlebar = NULL;
    }
    if (s_ctx.button_texture)
    {
        SDL_DestroyTexture(s_ctx.button_texture);
        s_ctx.button_texture = NULL;
    }
    if (s_ctx.back_icon)
    {
        SDL_DestroyTexture(s_ctx.back_icon);
        s_ctx.back_icon = NULL;
    }
    if (s_ctx.font_title)
    {
        TTF_CloseFont(s_ctx.font_title);
        s_ctx.font_title = NULL;
    }
    if (s_ctx.font_body)
    {
        TTF_CloseFont(s_ctx.font_body);
        s_ctx.font_body = NULL;
    }
    if (s_ctx.font_small)
    {
        TTF_CloseFont(s_ctx.font_small);
        s_ctx.font_small = NULL;
    }
    s_ctx.plant = NULL;
}

static void plantinfo_on_back(void *ud)
{
    (void)ud;
    scene_switch(SCENE_SELECT_PLANT);
}

static void plantinfo_on_select(void* userdata) {
    (void)userdata;
    // 현재 화면에서 선택한 식물 인덱스를 사용
    int idx = (int)(intptr_t)userdata;
    G_SelectedPlantIndex = idx;

    const PlantInfo* plant = plantdb_get(idx);
    if (plant && plant->id[0]) {
        SDL_strlcpy(G_Settings.gameplay.last_selected_plant, plant->id,
            sizeof(G_Settings.gameplay.last_selected_plant));
        settings_save(); // 저장 함수가 있다면 호출
    }

    // 로딩 컨텍스트 초기화
    s_loadCtx.step = 0;
    s_loadCtx.t0 = 0;

    // 로딩 시작: Gameplay로 전환
    loading_begin(SCENE_GAMEPLAY, gameplay_loading_job, &s_loadCtx, 0.15f, 0.15f);
}

/* Scene bridge */
static void init_bridge(void *arg)
{
    if (!plantinfo_init(arg))
        scene_switch(SCENE_SELECT_PLANT);
}

static void handle_bridge(SDL_Event *e)
{
    plantinfo_handle_event(e);
}

static void update_bridge(float dt)
{
    plantinfo_update(dt);
}

static void render_bridge(SDL_Renderer *r)
{
    (void)r;
    plantinfo_render();
}

static void cleanup_bridge(void)
{
    plantinfo_cleanup();
}

static Scene SCENE_OBJ = {init_bridge, handle_bridge, update_bridge, render_bridge, cleanup_bridge, "PlantInfo"};

Scene *scene_plantinfo_object(void)
{
    return &SCENE_OBJ;
}
