#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/settings.h"

typedef struct
{
    SDL_Rect bar;
    SDL_Rect handle;
    float value;
    int dragging;
} Slider;

static Slider bgm_slider;
static Slider sfx_slider;

static UIButton btn_bgm_toggle;
static UIButton btn_sfx_toggle;
static UIButton btn_test;
static UIButton btn_back;
static UIButton btn_save;

static SDL_Texture *s_bg = NULL;
static SDL_Texture *s_titlebarTex = NULL;
static SDL_Texture *s_backIcon = NULL;
static SDL_Texture *tex_button_basic = NULL;
static SDL_Texture *tex_bgm_track = NULL;
static SDL_Texture *tex_bgm_handle = NULL;
static SDL_Texture *tex_sfx_track = NULL;
static SDL_Texture *tex_sfx_handle = NULL;
static SDL_Texture *tex_save_icon = NULL;
static SDL_Texture *tex_music_on = NULL;
static SDL_Texture *tex_music_off = NULL;
static SDL_Texture *tex_volume_on = NULL;
static SDL_Texture *tex_volume_off = NULL;

static int s_titlebarW = 0;
static int s_titlebarH = 0;
static SDL_Rect s_panelRect = {0};
static SDL_Rect s_bgmHeaderRect = {0};
static SDL_Rect s_sfxHeaderRect = {0};
static SDL_Rect s_sfxTestRect = {0};

static TTF_Font *s_titleFont = NULL;
static TTF_Font *font = NULL;

static int point_in_rect(int x, int y, SDL_Rect rc)
{
    return (x >= rc.x && y >= rc.y && x < rc.x + rc.w && y < rc.y + rc.h);
}

static float clamp01f(float v)
{
    if (v < 0.f)
        return 0.f;
    if (v > 1.f)
        return 1.f;
    return v;
}

static void slider_sync_handle_from_value(Slider *s)
{
    int cx = s->bar.x + (int)(s->bar.w * s->value);
    s->handle.x = cx - s->handle.w / 2;
    s->handle.y = s->bar.y + (s->bar.h - s->handle.h) / 2;
}

static void update_toggle_labels(void)
{
    btn_bgm_toggle.text = NULL;
    btn_sfx_toggle.text = NULL;
}

static void settings_layout(int w, int h);
static void settings_render(SDL_Renderer *r);
static void on_toggle_bgm(void *ud);
static void on_toggle_sfx(void *ud);
static void on_test_sfx(void *ud);
static void on_back(void *ud);
static void on_save(void *ud);


static void settings_layout(int w, int h)
{
    const int margin = 60;
    int panelW = w - margin * 2;
    if (panelW > 960)
        panelW = 960;
    int panelH = h - (margin * 2 + 80);
    if (panelH < 500)
        panelH = 500;
    s_panelRect = (SDL_Rect){(w - panelW) / 2, margin + 50, panelW, panelH};

    int trackBH = 28;
    int handleBW = 40;
    int handleBH = 64;

    if (tex_bgm_track)
    {
        int tw, th;
        if (SDL_QueryTexture(tex_bgm_track, NULL, NULL, &tw, &th) == 0 && th > 0)
            trackBH = th;
    }
    if (tex_bgm_handle)
    {
        int tw, th;
        if (SDL_QueryTexture(tex_bgm_handle, NULL, NULL, &tw, &th) == 0)
        {
            if (tw > 0)
                handleBW = tw;
            if (th > 0)
                handleBH = th;
        }
    }

    int toggleW = 140;
    int toggleH = 80;
    if (tex_music_on)
    {
        int tw, th;
        if (SDL_QueryTexture(tex_music_on, NULL, NULL, &tw, &th) == 0 && tw > 0 && th > 0)
        {
            toggleW = tw;
            toggleH = th;
        }
    }

    int contentLeft = s_panelRect.x + 80;
    int sliderX = contentLeft + toggleW + 40;
    int sliderWidth = s_panelRect.x + s_panelRect.w - sliderX - 140;
    if (sliderWidth < 320)
        sliderWidth = 320;

    int bgmCenterY = s_panelRect.y + 220;
    btn_bgm_toggle.r = (SDL_Rect){contentLeft, bgmCenterY - toggleH / 2, toggleW, toggleH};
    bgm_slider.bar = (SDL_Rect){sliderX, bgmCenterY - trackBH / 2, sliderWidth, trackBH};
    bgm_slider.handle.w = handleBW;
    bgm_slider.handle.h = handleBH;
    slider_sync_handle_from_value(&bgm_slider);

    int sfxCenterY = bgmCenterY + toggleH + 200;
    int sfxToggleW = toggleW;
    int sfxToggleH = toggleH;
    if (tex_volume_on)
    {
        int tw, th;
        if (SDL_QueryTexture(tex_volume_on, NULL, NULL, &tw, &th) == 0 && tw > 0 && th > 0)
        {
            sfxToggleW = tw;
            sfxToggleH = th;
        }
    }
    btn_sfx_toggle.r = (SDL_Rect){contentLeft, sfxCenterY - sfxToggleH / 2, sfxToggleW, sfxToggleH};

    int sfxTrackH = trackBH;
    int sfxHandleW = handleBW;
    int sfxHandleH = handleBH;
    if (tex_sfx_track)
    {
        int tw, th;
        if (SDL_QueryTexture(tex_sfx_track, NULL, NULL, &tw, &th) == 0 && th > 0)
            sfxTrackH = th;
    }
    if (tex_sfx_handle)
    {
        int tw, th;
        if (SDL_QueryTexture(tex_sfx_handle, NULL, NULL, &tw, &th) == 0)
        {
            if (tw > 0)
                sfxHandleW = tw;
            if (th > 0)
                sfxHandleH = th;
        }
    }
    sfx_slider.bar = (SDL_Rect){sliderX, sfxCenterY - sfxTrackH / 2, sliderWidth, sfxTrackH};
    sfx_slider.handle.w = sfxHandleW;
    sfx_slider.handle.h = sfxHandleH;
    slider_sync_handle_from_value(&sfx_slider);

    int headerHeight = 66;
    s_bgmHeaderRect = (SDL_Rect){sliderX, bgm_slider.bar.y - headerHeight - 24, sliderWidth, headerHeight};

    int headerSpacing = 24;
    int testWidth = 240;
    if (testWidth > sliderWidth - 120)
        testWidth = sliderWidth / 2;
    s_sfxHeaderRect = (SDL_Rect){sliderX, sfx_slider.bar.y - headerHeight - 24, sliderWidth - testWidth - headerSpacing, headerHeight};
    if (s_sfxHeaderRect.w < 180)
        s_sfxHeaderRect.w = 180;
    s_sfxTestRect = (SDL_Rect){s_sfxHeaderRect.x + s_sfxHeaderRect.w + headerSpacing, s_sfxHeaderRect.y, testWidth, headerHeight};

    btn_test.r = s_sfxTestRect;

    int saveSize = 72;
    int saveX = s_panelRect.x + s_panelRect.w - saveSize - 40;
    int saveY = s_panelRect.y + s_panelRect.h - saveSize - 30;
    btn_save.r = (SDL_Rect){saveX, saveY, saveSize, saveSize};

    btn_back.r = (SDL_Rect){40, 40, 72, 72};
}

static void draw_text(SDL_Renderer *r, TTF_Font *f, SDL_Color c, int x, int y, const char *text)
{
    if (!f || !text || !*text)
        return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, text, c);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_slider(SDL_Renderer *r, Slider *s, SDL_Texture *track, SDL_Texture *handle)
{
    if (track)
    {
        SDL_RenderCopy(r, track, NULL, &s->bar);
    }
    else
    {
        SDL_SetRenderDrawColor(r, 102, 78, 58, 255);
        SDL_RenderFillRect(r, &s->bar);
        SDL_SetRenderDrawColor(r, 64, 46, 32, 255);
        SDL_RenderDrawRect(r, &s->bar);
    }

    if (handle)
    {
        SDL_RenderCopy(r, handle, NULL, &s->handle);
    }
    else
    {
        SDL_Rect handleDst = s->handle;
        SDL_SetRenderDrawColor(r, 235, 220, 180, 255);
        SDL_RenderFillRect(r, &handleDst);
        SDL_SetRenderDrawColor(r, 96, 64, 40, 255);
        SDL_RenderDrawRect(r, &handleDst);
    }
}

static void settings_init(void *arg)
{
    (void)arg;
    int w, h;
    SDL_GetRendererOutputSize(G_Renderer, &w, &h);

    bgm_slider.dragging = 0;
    bgm_slider.value = clamp01f(G_Settings.audio.bgm_volume);
    sfx_slider.dragging = 0;
    sfx_slider.value = clamp01f(G_Settings.audio.sfx_volume);

    if (!font)
    {
        font = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 28);
        if (!font)
            SDL_Log("Settings font load failed: %s", TTF_GetError());
    }

    if (!s_titleFont)
    {
        s_titleFont = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 46);
        if (!s_titleFont)
            SDL_Log("Settings title font load failed: %s", TTF_GetError());
    }

    if (!s_bg)
    {
        s_bg = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
        if (!s_bg)
            SDL_Log("Settings background load failed: %s", IMG_GetError());
    }

    if (!s_titlebarTex)
    {
        s_titlebarTex = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_titlebar.png");
        if (!s_titlebarTex)
            SDL_Log("Settings titlebar load failed: %s", IMG_GetError());
        else
            SDL_QueryTexture(s_titlebarTex, NULL, NULL, &s_titlebarW, &s_titlebarH);
    }

    if (!s_backIcon)
    {
        s_backIcon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
        if (!s_backIcon)
            SDL_Log("Settings back icon load failed: %s", IMG_GetError());
    }

    if (!tex_button_basic)
    {
        tex_button_basic = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_basic.png");
        if (!tex_button_basic)
            SDL_Log("Settings button texture load failed: %s", IMG_GetError());
    }

    if (!tex_bgm_track)
    {
        tex_bgm_track = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "Y_musicbar.png");
        if (!tex_bgm_track)
            SDL_Log("Settings BGM track load failed: %s", IMG_GetError());
    }

    if (!tex_bgm_handle)
    {
        tex_bgm_handle = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_musicToggle.png");
        if (!tex_bgm_handle)
            SDL_Log("Settings BGM handle load failed: %s", IMG_GetError());
    }

    if (!tex_sfx_track)
    {
        tex_sfx_track = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "Y_volumebar.png");
        if (!tex_sfx_track)
            SDL_Log("Settings SFX track load failed: %s", IMG_GetError());
    }

    if (!tex_sfx_handle)
    {
        tex_sfx_handle = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_musicToggle.png");
        if (!tex_sfx_handle)
            SDL_Log("Settings SFX handle load failed: %s", IMG_GetError());
    }

    if (!tex_save_icon)
    {
        tex_save_icon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_check.png");
        if (!tex_save_icon)
            SDL_Log("Settings save icon load failed: %s", IMG_GetError());
    }

    if (!tex_music_on)
    {
        tex_music_on = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "Y_musicON.png");
        if (!tex_music_on)
            SDL_Log("Settings music ON icon load failed: %s", IMG_GetError());
    }
    if (!tex_music_off)
    {
        tex_music_off = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "Y_musicOFF.png");
        if (!tex_music_off)
            SDL_Log("Settings music OFF icon load failed: %s", IMG_GetError());
    }
    if (!tex_volume_on)
    {
        tex_volume_on = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "Y_volumeON.png");
        if (!tex_volume_on)
            SDL_Log("Settings volume ON icon load failed: %s", IMG_GetError());
    }
    if (!tex_volume_off)
    {
        tex_volume_off = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "Y_volumeOFF.png");
        if (!tex_volume_off)
            SDL_Log("Settings volume OFF icon load failed: %s", IMG_GetError());
    }

    ui_button_init(&btn_bgm_toggle, (SDL_Rect){0, 0, 0, 0}, NULL);
    ui_button_set_callback(&btn_bgm_toggle, on_toggle_bgm, NULL);
    ui_button_set_sfx(&btn_bgm_toggle, G_SFX_Click, G_SFX_Hover);

    ui_button_init(&btn_sfx_toggle, (SDL_Rect){0, 0, 0, 0}, NULL);
    ui_button_set_callback(&btn_sfx_toggle, on_toggle_sfx, NULL);
    ui_button_set_sfx(&btn_sfx_toggle, G_SFX_Click, G_SFX_Hover);

    ui_button_init(&btn_test, (SDL_Rect){0, 0, 0, 0}, "효과음 테스트");
    ui_button_set_callback(&btn_test, on_test_sfx, NULL);
    ui_button_set_sfx(&btn_test, G_SFX_Click, G_SFX_Hover);

    ui_button_init(&btn_back, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&btn_back, on_back, NULL);
    ui_button_set_sfx(&btn_back, G_SFX_Click, G_SFX_Hover);

    ui_button_init(&btn_save, (SDL_Rect){0, 0, 0, 0}, "");
    ui_button_set_callback(&btn_save, on_save, NULL);
    ui_button_set_sfx(&btn_save, G_SFX_Click, G_SFX_Hover);

    update_toggle_labels();

    settings_layout(w, h);
}

static void on_toggle_bgm(void *ud)
{
    (void)ud;
    G_Settings.audio.mute_bgm = !G_Settings.audio.mute_bgm;
    settings_apply_audio();
    update_toggle_labels();
}

static void on_toggle_sfx(void *ud)
{
    (void)ud;
    G_Settings.audio.mute_sfx = !G_Settings.audio.mute_sfx;
    settings_apply_audio();
    update_toggle_labels();
}

static void on_test_sfx(void *ud)
{
    (void)ud;
    if (!G_Settings.audio.mute_sfx && G_SFX_Click)
        Mix_PlayChannel(-1, G_SFX_Click, 0);
}

static void on_back(void *ud)
{
    (void)ud;
    scene_switch(SCENE_MAINMENU);
}

static void on_save(void *ud)
{
    (void)ud;
    settings_save();
    scene_switch(SCENE_MAINMENU);
}

static void settings_render(SDL_Renderer *r)
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

    SDL_SetRenderDrawColor(r, 179, 139, 98, 255);
    SDL_RenderFillRect(r, &s_panelRect);
    SDL_SetRenderDrawColor(r, 92, 63, 44, 255);
    SDL_RenderDrawRect(r, &s_panelRect);

    SDL_Rect inner = {s_panelRect.x + 24, s_panelRect.y + 24, s_panelRect.w - 48, s_panelRect.h - 48};
    SDL_SetRenderDrawColor(r, 236, 216, 188, 255);
    SDL_RenderFillRect(r, &inner);
    SDL_SetRenderDrawColor(r, 141, 101, 66, 160);
    SDL_RenderDrawRect(r, &inner);

    if (s_titleFont)
    {
        const char *title = "설정";
        SDL_Surface *surf = TTF_RenderUTF8_Blended(s_titleFont, title, (SDL_Color){245, 245, 240, 255});
        if (surf)
        {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
            int tw = surf->w;
            int th = surf->h;
            int scaledW = (int)(tw * 1.4f + 0.5f);
            int scaledH = (int)(th * 1.4f + 0.5f);
            if (scaledW < 1)
                scaledW = 1;
            if (scaledH < 1)
                scaledH = 1;

            int centerX = s_panelRect.x + s_panelRect.w / 2;
            int titleY = s_panelRect.y - 40;

            if (s_titlebarTex && s_titlebarW > 0 && s_titlebarH > 0)
            {
                int desiredWidth = scaledW + 220;
                int scale = (desiredWidth + s_titlebarW - 1) / s_titlebarW;
                if (scale < 1)
                    scale = 1;
                SDL_Rect barDst = {centerX - (s_titlebarW * scale) / 2, titleY - (s_titlebarH * scale) / 2, s_titlebarW * scale, s_titlebarH * scale};
                SDL_RenderCopy(r, s_titlebarTex, NULL, &barDst);
            }

            SDL_Rect dst = {centerX - scaledW / 2, titleY - scaledH / 2, scaledW, scaledH};
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    SDL_Color labelColor = {245, 245, 240, 255};
    SDL_Color valueColor = {222, 204, 178, 255};

    SDL_Rect bgmHeaderRect = s_bgmHeaderRect;
    SDL_Rect sfxHeaderRect = s_sfxHeaderRect;
    if (tex_button_basic)
    {
        SDL_RenderCopy(r, tex_button_basic, NULL, &bgmHeaderRect);
        SDL_RenderCopy(r, tex_button_basic, NULL, &sfxHeaderRect);
    }
    else
    {
        SDL_SetRenderDrawColor(r, 200, 180, 150, 255);
        SDL_RenderFillRect(r, &bgmHeaderRect);
        SDL_RenderFillRect(r, &sfxHeaderRect);
        SDL_SetRenderDrawColor(r, 120, 90, 60, 255);
        SDL_RenderDrawRect(r, &bgmHeaderRect);
        SDL_RenderDrawRect(r, &sfxHeaderRect);
    }
    draw_text(r, font ? font : G_FontMain, labelColor, bgmHeaderRect.x + 20, bgmHeaderRect.y + bgmHeaderRect.h / 2 - 14, "BGM 설정");
    draw_text(r, font ? font : G_FontMain, labelColor, sfxHeaderRect.x + 20, sfxHeaderRect.y + sfxHeaderRect.h / 2 - 14, "효과음 설정");

    char volBuf[32];
    SDL_snprintf(volBuf, sizeof(volBuf), "%d%%", (int)(bgm_slider.value * 100.f + 0.5f));
    draw_slider(r, &bgm_slider, tex_bgm_track, tex_bgm_handle);
    draw_text(r, font ? font : G_FontMain, valueColor, bgm_slider.bar.x + bgm_slider.bar.w + 20, bgm_slider.bar.y + bgm_slider.bar.h / 2 - 12, volBuf);

    SDL_snprintf(volBuf, sizeof(volBuf), "%d%%", (int)(sfx_slider.value * 100.f + 0.5f));
    draw_slider(r, &sfx_slider, tex_sfx_track, tex_sfx_handle);
    draw_text(r, font ? font : G_FontMain, valueColor, sfx_slider.bar.x + sfx_slider.bar.w + 20, sfx_slider.bar.y + sfx_slider.bar.h / 2 - 12, volBuf);

    SDL_Texture *bgmToggleTex = G_Settings.audio.mute_bgm ? tex_music_off : tex_music_on;
    SDL_Texture *sfxToggleTex = G_Settings.audio.mute_sfx ? tex_volume_off : tex_volume_on;
    if (bgmToggleTex)
        SDL_RenderCopy(r, bgmToggleTex, NULL, &btn_bgm_toggle.r);
    else
    {
        SDL_SetRenderDrawColor(r, 200, 180, 150, 255);
        SDL_RenderFillRect(r, &btn_bgm_toggle.r);
        SDL_SetRenderDrawColor(r, 120, 90, 60, 255);
        SDL_RenderDrawRect(r, &btn_bgm_toggle.r);
    }
    if (btn_bgm_toggle.hovered)
    {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 140);
        SDL_RenderDrawRect(r, &btn_bgm_toggle.r);
    }
    if (sfxToggleTex)
        SDL_RenderCopy(r, sfxToggleTex, NULL, &btn_sfx_toggle.r);
    else
    {
        SDL_SetRenderDrawColor(r, 200, 180, 150, 255);
        SDL_RenderFillRect(r, &btn_sfx_toggle.r);
        SDL_SetRenderDrawColor(r, 120, 90, 60, 255);
        SDL_RenderDrawRect(r, &btn_sfx_toggle.r);
    }
    if (btn_sfx_toggle.hovered)
    {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 140);
        SDL_RenderDrawRect(r, &btn_sfx_toggle.r);
    }
    ui_button_render(r, font, &btn_test, tex_button_basic);
    ui_button_render(r, NULL, &btn_back, s_backIcon);
    ui_button_render(r, NULL, &btn_save, tex_save_icon);
}

static void handle(SDL_Event *e)
{
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    ui_button_handle(&btn_bgm_toggle, e);
    ui_button_handle(&btn_sfx_toggle, e);
    ui_button_handle(&btn_test, e);
    ui_button_handle(&btn_back, e);
    ui_button_handle(&btn_save, e);

    if (e->type == SDL_QUIT)
    {
        G_Running = 0;
        return;
    }

    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE)
    {
        scene_switch(SCENE_MAINMENU);
        return;
    }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
    {
        settings_layout(e->window.data1, e->window.data2);
        return;
    }

    switch (e->type)
    {
    case SDL_MOUSEBUTTONDOWN:
        if (e->button.button == SDL_BUTTON_LEFT)
        {
            if (point_in_rect(mx, my, bgm_slider.handle))
            {
                bgm_slider.dragging = 1;
                G_Settings.audio.bgm_volume = bgm_slider.value;
                settings_apply_audio();
            }
            else if (point_in_rect(mx, my, bgm_slider.bar))
            {
                float pos = (float)(mx - bgm_slider.bar.x) / (float)bgm_slider.bar.w;
                pos = clamp01f(pos);
                bgm_slider.value = pos;
                slider_sync_handle_from_value(&bgm_slider);
                bgm_slider.dragging = 1;
                G_Settings.audio.bgm_volume = bgm_slider.value;
                settings_apply_audio();
            }

            if (point_in_rect(mx, my, sfx_slider.handle))
            {
                sfx_slider.dragging = 1;
                G_Settings.audio.sfx_volume = sfx_slider.value;
                settings_apply_audio();
            }
            else if (point_in_rect(mx, my, sfx_slider.bar))
            {
                float pos = clamp01f((float)(mx - sfx_slider.bar.x) / (float)sfx_slider.bar.w);
                sfx_slider.value = pos;
                sfx_slider.dragging = 1;
                slider_sync_handle_from_value(&sfx_slider);
                G_Settings.audio.sfx_volume = sfx_slider.value;
                settings_apply_audio();
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (e->button.button == SDL_BUTTON_LEFT)
        {
            if (bgm_slider.dragging)
            {
                G_Settings.audio.bgm_volume = bgm_slider.value;
                settings_apply_audio();
            }
            bgm_slider.dragging = 0;
            if (sfx_slider.dragging)
            {
                G_Settings.audio.sfx_volume = sfx_slider.value;
                settings_apply_audio();
            }
            sfx_slider.dragging = 0;
        }
        break;

    case SDL_MOUSEMOTION:
        if (bgm_slider.dragging)
        {
            float pos = (float)(mx - bgm_slider.bar.x) / (float)bgm_slider.bar.w;
            pos = clamp01f(pos);
            bgm_slider.value = pos;
            slider_sync_handle_from_value(&bgm_slider);
            G_Settings.audio.bgm_volume = bgm_slider.value;
            settings_apply_audio();
        }
        if (sfx_slider.dragging)
        {
            float pos = clamp01f((float)(mx - sfx_slider.bar.x) / (float)sfx_slider.bar.w);
            sfx_slider.value = pos;
            slider_sync_handle_from_value(&sfx_slider);
            G_Settings.audio.sfx_volume = sfx_slider.value;
            settings_apply_audio();
        }
        break;
    }
}

static void update(float dt)
{
    (void)dt;
}

static void render(SDL_Renderer *r)
{
    settings_render(r);
}

static void cleanup(void)
{
    if (s_bg)
    {
        SDL_DestroyTexture(s_bg);
        s_bg = NULL;
    }
    if (s_titlebarTex)
    {
        SDL_DestroyTexture(s_titlebarTex);
        s_titlebarTex = NULL;
    }
    if (s_backIcon)
    {
        SDL_DestroyTexture(s_backIcon);
        s_backIcon = NULL;
    }
    if (tex_button_basic)
    {
        SDL_DestroyTexture(tex_button_basic);
        tex_button_basic = NULL;
    }
    if (tex_bgm_track)
    {
        SDL_DestroyTexture(tex_bgm_track);
        tex_bgm_track = NULL;
    }
    if (tex_bgm_handle)
    {
        SDL_DestroyTexture(tex_bgm_handle);
        tex_bgm_handle = NULL;
    }
    if (tex_sfx_track)
    {
        SDL_DestroyTexture(tex_sfx_track);
        tex_sfx_track = NULL;
    }
    if (tex_sfx_handle)
    {
        SDL_DestroyTexture(tex_sfx_handle);
        tex_sfx_handle = NULL;
    }
    if (tex_save_icon)
    {
        SDL_DestroyTexture(tex_save_icon);
        tex_save_icon = NULL;
    }
    if (tex_music_on)
    {
        SDL_DestroyTexture(tex_music_on);
        tex_music_on = NULL;
    }
    if (tex_music_off)
    {
        SDL_DestroyTexture(tex_music_off);
        tex_music_off = NULL;
    }
    if (tex_volume_on)
    {
        SDL_DestroyTexture(tex_volume_on);
        tex_volume_on = NULL;
    }
    if (tex_volume_off)
    {
        SDL_DestroyTexture(tex_volume_off);
        tex_volume_off = NULL;
    }
    if (s_titleFont)
    {
        TTF_CloseFont(s_titleFont);
        s_titleFont = NULL;
    }
    if (font)
    {
        TTF_CloseFont(font);
        font = NULL;
    }
}

static Scene SCENE_OBJ = {settings_init, handle, update, render, cleanup, "Settings"};

Scene *scene_settings_object(void)
{
    return &SCENE_OBJ;
}
