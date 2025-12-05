#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/loading.h"
#include "../include/gameplay.h"
#include "../include/core.h"
#include "../include/settings.h"
#include <stdbool.h>
#include <parson.h>
#define BTN_COUNT 6

static UIButton s_buttons[BTN_COUNT];
static Mix_Chunk *sfx_click = NULL;
static Mix_Chunk *sfx_hover = NULL;
Mix_Music* G_MenuMusic = NULL;

typedef struct
{
    SDL_Rect rect;
    int duration_ms;
    SDL_Texture *texture;
} AnimFrame;

static SDL_Texture *s_bgAtlas = NULL;
static AnimFrame s_bgFrames[128];
static int s_bgFrameCount = 0;
static int s_bgFrameIndex = 0;
static float s_bgFrameElapsedMs = 0.f;
static bool s_bgFramesUseAtlas = false;

extern LoadCtx s_loadCtx; // gameplay.c에 '정의'가 있으니 여기선 extern
extern int gameplay_loading_job(void* userdata, float* out_progress);
extern int plantdb_find_index_by_id(const char* id); // 네가 가진 유틸로 맞춰 쓰면 됨

static void destroy_frame_textures(void)
{
    for (int i = 0; i < (int)SDL_arraysize(s_bgFrames); ++i)
    {
        if (s_bgFrames[i].texture)
        {
            SDL_DestroyTexture(s_bgFrames[i].texture);
            s_bgFrames[i].texture = NULL;
        }
    }
}

static void on_start(void *ud)
{
    (void)ud;
    scene_switch_fade(SCENE_SELECT_PLANT, 0.2f, 0.4f);
}

static void on_continue(void *ud)
{
    (void)ud;

    const char* last_id = G_Settings.gameplay.last_selected_plant;
    int idx = plantdb_find_index_by_id(last_id);
    if (idx < 0) {
        SDL_Log("[CONTINUE] invalid plant id '%s', fallback to select scene", last_id);
        scene_switch(SCENE_SELECT_PLANT);
        return;
    }

    // 2️⃣ 선택 인덱스 전역 반영
    G_SelectedPlantIndex = idx;
    SDL_Log("[CONTINUE] continue plant index=%d (%s)", idx, last_id);

    // 3️⃣ 로딩씬 시작
    s_loadCtx.step = 0;
    s_loadCtx.t0 = 0;

    loading_begin(SCENE_GAMEPLAY, gameplay_loading_job, &s_loadCtx, 0.15f, 0.15f);
}
static void on_codex(void *ud)
{
    (void)ud;
    scene_switch_fade(SCENE_CODEX, 0.2f, 0.4f);
}
static void on_settings(void *ud)
{
    (void)ud;
    scene_switch(SCENE_SETTINGS);
}
static void on_credits(void *ud)
{
    (void)ud;
    scene_switch_fade(SCENE_CREDITS, 0.2f, 0.4f);
}
static void on_exit(void *ud)
{
    (void)ud;
    G_Running = 0;
}

static SDL_Texture *s_title = NULL;
static SDL_Texture *s_play = NULL;
static SDL_Texture *s_continue = NULL;
static SDL_Texture *s_settings = NULL;
static SDL_Texture *s_credits = NULL;
static SDL_Texture *s_quit = NULL;
static SDL_Texture *s_collection = NULL;

UIButton btn_play;

static TTF_Font *s_titleFont = NULL;

SDL_Texture *s_button_img[6];

static void layout(void)
{
    int w, h;
    SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    int bw = (int)(w * 0.25f), bh = 78, gap = 20;
    int total = 5 * bh + 4 * gap;
    int x = (w - bw) / 2, y = ((h - total) / 2 + h / 20) + 20;

    // const char* labels[BTN_COUNT] = {"시작", "이어하기","도감","설정","크레딧","종료" };
    UIButtonOnClick funcs[BTN_COUNT] = {on_start, on_continue, on_codex, on_settings, on_credits, on_exit};

    for (int i = 0; i < BTN_COUNT; i++)
    {
        ui_button_init(&s_buttons[i], (SDL_Rect){x, y + i * (bh + gap), bw, bh}, NULL);

        ui_button_set_callback(&s_buttons[i], funcs[i], NULL);
        ui_button_set_sfx(&s_buttons[i], sfx_click, sfx_hover);
    }
}

static void load_background_animation(void)
{
    destroy_frame_textures();

    s_bgFrameCount = 0;
    s_bgFrameIndex = 0;
    s_bgFrameElapsedMs = 0.f;
    s_bgFramesUseAtlas = false;

    if (s_bgAtlas)
    {
        SDL_DestroyTexture(s_bgAtlas);
        s_bgAtlas = NULL;
    }

    s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "mainscene.png");
    if (!s_bgAtlas)
    {
        SDL_Log("[MAINMENU] Load mainscene.png failed: %s", IMG_GetError());
    }

    JSON_Value *root = json_parse_file(ASSETS_DIR "data/mainscene.json");
    if (!root)
    {
        SDL_Log("[MAINMENU] Parse mainscene.json failed");
        return;
    }

    JSON_Object *rootObj = json_value_get_object(root);
    JSON_Object *framesObj = rootObj ? json_object_get_object(rootObj, "frames") : NULL;
    if (!framesObj)
    {
        SDL_Log("[MAINMENU] mainscene.json missing frames object");
        json_value_free(root);
        return;
    }

    for (int i = 0; i < (int)SDL_arraysize(s_bgFrames); ++i)
    {
        char key[32];
        SDL_snprintf(key, sizeof(key), "mainscene %d.gif", i);
        JSON_Object *frameObj = json_object_get_object(framesObj, key);
        if (!frameObj)
        {
            SDL_snprintf(key, sizeof(key), "mainscene %d.aseprite", i);
            frameObj = json_object_get_object(framesObj, key);
        }
        if (!frameObj)
            break;

        JSON_Object *rectObj = json_object_get_object(frameObj, "frame");
        if (!rectObj)
            continue;

        AnimFrame frame = {0};
        frame.rect.x = (int)json_object_get_number(rectObj, "x");
        frame.rect.y = (int)json_object_get_number(rectObj, "y");
        frame.rect.w = (int)json_object_get_number(rectObj, "w");
        frame.rect.h = (int)json_object_get_number(rectObj, "h");
        frame.texture = NULL;

        double duration = json_object_get_number(frameObj, "duration");
        frame.duration_ms = duration > 0 ? (int)duration : 100;

        if (frame.rect.w <= 0 || frame.rect.h <= 0)
            continue;

        s_bgFrames[s_bgFrameCount++] = frame;
    }

    json_value_free(root);

    if (s_bgFrameCount == 0)
    {
        SDL_Log("[MAINMENU] mainscene.json parsed but no valid frames");
        return;
    }

    if (s_bgAtlas)
    {
        s_bgFramesUseAtlas = true;
        return;
    }

    SDL_Surface *atlasSurface = IMG_Load(ASSETS_IMAGES_DIR "mainscene.png");
    if (!atlasSurface)
    {
        SDL_Log("[MAINMENU] Load mainscene.png as surface failed: %s", IMG_GetError());
        s_bgFrameCount = 0;
        return;
    }

    if (atlasSurface->format->format != SDL_PIXELFORMAT_RGBA32)
    {
        SDL_Surface *converted = SDL_ConvertSurfaceFormat(atlasSurface, SDL_PIXELFORMAT_RGBA32, 0);
        if (!converted)
        {
            SDL_Log("[MAINMENU] Convert mainscene surface failed: %s", SDL_GetError());
            SDL_FreeSurface(atlasSurface);
            s_bgFrameCount = 0;
            return;
        }
        SDL_FreeSurface(atlasSurface);
        atlasSurface = converted;
    }

    for (int i = 0; i < s_bgFrameCount; ++i)
    {
        AnimFrame *frame = &s_bgFrames[i];
        SDL_Surface *frameSurface = SDL_CreateRGBSurfaceWithFormat(0, frame->rect.w, frame->rect.h, 32, atlasSurface->format->format);
        if (!frameSurface)
        {
            SDL_Log("[MAINMENU] Create frame surface failed: %s", SDL_GetError());
            continue;
        }

        SDL_Rect src = frame->rect;
        if (SDL_BlitSurface(atlasSurface, &src, frameSurface, NULL) < 0)
        {
            SDL_Log("[MAINMENU] Blit frame surface failed: %s", SDL_GetError());
            SDL_FreeSurface(frameSurface);
            continue;
        }

        SDL_Texture *tex = SDL_CreateTextureFromSurface(G_Renderer, frameSurface);
        SDL_FreeSurface(frameSurface);

        if (!tex)
        {
            SDL_Log("[MAINMENU] Create frame texture failed: %s", SDL_GetError());
            continue;
        }

        frame->texture = tex;
    }

    SDL_FreeSurface(atlasSurface);

    int writeIndex = 0;
    for (int readIndex = 0; readIndex < s_bgFrameCount; ++readIndex)
    {
        if (!s_bgFrames[readIndex].texture)
            continue;

        if (writeIndex != readIndex)
            s_bgFrames[writeIndex] = s_bgFrames[readIndex];

        ++writeIndex;
    }
    s_bgFrameCount = writeIndex;

    if (s_bgFrameCount == 0)
    {
        SDL_Log("[MAINMENU] No usable frame textures produced for mainscene");
        return;
    }

    SDL_Log("[MAINMENU] Using fallback frame textures for mainscene animation");
}

static void init(void *arg)
{
    (void)arg;
    sfx_click = G_SFX_Click;
    sfx_hover = G_SFX_Hover ? G_SFX_Hover : sfx_click;

    

    if (!G_MenuMusic) G_MenuMusic = Mix_LoadMUS(ASSETS_SOUNDS_DIR "Morning_twilight.wav");
    if (G_MenuMusic) {
        // 루프 계속
        if (!Mix_PlayingMusic()) Mix_FadeInMusic(G_MenuMusic, -1, 300);
    }
    settings_apply_audio();

    layout();
    int want = IMG_INIT_PNG;
    if ((IMG_Init(want) & want) != want)
    {
        SDL_Log("IMG_Init: %s", IMG_GetError());
    }

    load_background_animation();

    s_play = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_start.png");
    if (!s_play)
    {
        SDL_Log("Load btn_play.png failed: %s", IMG_GetError());
    }

    s_continue = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_continue.png");
    if (!s_continue)
    {
        SDL_Log("Load btn_continue.png failed: %s", IMG_GetError());
    }
    s_collection = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_collection.png");
    if (!s_collection)
    {
        SDL_Log("Load btn_collection.png failed: %s", IMG_GetError());
    }
    s_settings = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_setting.png");
    if (!s_settings)
    {
        SDL_Log("Load btn_settings.png failed: %s", IMG_GetError());
    }
    s_credits = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_credit.png");
    if (!s_credits)
    {
        SDL_Log("Load btn_credits.png failed: %s", IMG_GetError());
    }
    s_quit = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_quit.png");
    if (!s_quit)
    {
        SDL_Log("Load btn_quit.png failed: %s", IMG_GetError());
    }

    s_button_img[0] = s_play;
    s_button_img[1] = s_continue;
    s_button_img[2] = s_collection;
    s_button_img[3] = s_settings;
    s_button_img[4] = s_credits;
    s_button_img[5] = s_quit;

    s_title = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "title.png");
    if (!s_title)
    {
        SDL_Log("Load title.png failed: %s", IMG_GetError());
    }

    s_titleFont = TTF_OpenFont(ASSETS_FONTS_DIR "NeoDunggeunmoPro-Regular.ttf", 48);
    if (!s_titleFont)
    {
        SDL_Log("TitleFont Open: %s", TTF_GetError());
    }

    SDL_Log("[MAINMENU] TTF_WasInit=%d", TTF_WasInit());
    {
        const char *p = ASSETS_FONTS_DIR "NeoDunggeunmoPro-Regular.ttf";
        SDL_RWops *rw = SDL_RWFromFile(p, "rb");
        SDL_Log("[MAINMENU] font path: %s, exist=%s", p, rw ? "YES" : "NO");
        if (rw)
            SDL_RWclose(rw);
    }

    if (!s_titleFont && !G_FontMain)
    {
        SDL_Log("[MAINMENU] No font available (both title and global NULL)");
    }

    SDL_Texture* texplayHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_start_hover.png");
    SDL_Texture* texplayPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_start_pressed.png");

    ui_button_set_icons(&s_buttons[0], s_play, texplayHover, texplayPressed);

    SDL_Texture* texcontinueHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_continue_hover.png");
    SDL_Texture* texcontinuePressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_continue_pressed.png");

    ui_button_set_icons(&s_buttons[1], s_continue, texcontinueHover, texcontinuePressed);

    SDL_Texture* texcollectionHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_collection_hover.png");
    SDL_Texture* texcollectionPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_collection_pressed.png");

    ui_button_set_icons(&s_buttons[2], s_collection, texcollectionHover, texcollectionPressed);

    SDL_Texture* texsettingsHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_setting_hover.png");
    SDL_Texture* texsettingsPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_setting_pressed.png");

    ui_button_set_icons(&s_buttons[3], s_settings, texsettingsHover, texsettingsPressed);

    SDL_Texture* texcreditsHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_credit_hover.png");
    SDL_Texture* texcreditsPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_credit_pressed.png");

    ui_button_set_icons(&s_buttons[4], s_credits, texcreditsHover, texcreditsPressed);

    SDL_Texture* texexitHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_quit_hover.png");
    SDL_Texture* texexitPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "B_quit_pressed.png");

    ui_button_set_icons(&s_buttons[5], s_quit, texexitHover, texexitPressed);

    // render() 타이틀 그리기 직전에
    SDL_Log("[MAINMENU] render title try");

    
    
}
static void handle(SDL_Event *e)
{
    if (e->type == SDL_QUIT)
    {
        G_Running = 0;
        return;
    }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        layout();
    for (int i = 0; i < BTN_COUNT; i++)
        ui_button_handle(&s_buttons[i], e);
}
static void update(float dt)
{
    if (s_bgFrameCount <= 0)
        return;

    s_bgFrameElapsedMs += dt * 1000.f;

    int duration = s_bgFrames[s_bgFrameIndex].duration_ms;
    if (duration <= 0)
        duration = 100;

    while (s_bgFrameElapsedMs >= duration && s_bgFrameCount > 0)
    {
        s_bgFrameElapsedMs -= duration;
        s_bgFrameIndex = (s_bgFrameIndex + 1) % s_bgFrameCount;

        duration = s_bgFrames[s_bgFrameIndex].duration_ms;
        if (duration <= 0)
            duration = 100;
    }
}
static void render(SDL_Renderer *r)
{
    // 0) 매 프레임 시작할 때만 Clear
    SDL_SetRenderDrawColor(r, 16, 20, 28, 255);

    // 1) 배경
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);
    bool usingAnimatedBg = false;
    if (s_bgFrameCount > 0)
    {
        if (s_bgFramesUseAtlas && s_bgAtlas)
            usingAnimatedBg = true;
        else if (!s_bgFramesUseAtlas && s_bgFrames[s_bgFrameIndex].texture)
            usingAnimatedBg = true;
    }

    if (usingAnimatedBg)
    {
        SDL_Rect dst = {0, 0, w, h};
        if (s_bgFramesUseAtlas && s_bgAtlas)
        {
            SDL_Rect src = s_bgFrames[s_bgFrameIndex].rect;
            SDL_RenderCopy(r, s_bgAtlas, &src, &dst);
        }
        else
        {
            SDL_Texture *frameTex = s_bgFrames[s_bgFrameIndex].texture;
            if (frameTex)
            {
                SDL_RenderCopy(r, frameTex, NULL, &dst);
            }
        }
    }
    else if (s_bgAtlas)
    {
        SDL_Rect dst = {0, 0, w, h};
        SDL_RenderCopy(r, s_bgAtlas, NULL, &dst);
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

    // 2) 타이틀
    TTF_Font *titleFont = s_titleFont ? s_titleFont : G_FontMain;
    if (titleFont)
    {
        SDL_Color shadow = {0, 0, 0, 160};
        SDL_Color mainC = {235, 245, 235, 255};

        if (s_title)
        {
            SDL_Rect dst = {0, 0, w, h};
            SDL_RenderCopy(r, s_title, NULL, &dst);
        }
        /*
        SDL_Surface* s1 = TTF_RenderUTF8_Blended(titleFont, title, shadow);
        SDL_Surface* s2 = TTF_RenderUTF8_Blended(titleFont, title, mainC);
        if (s1 && s2) {
            SDL_Texture* t1 = SDL_CreateTextureFromSurface(r, s1);
            SDL_Texture* t2 = SDL_CreateTextureFromSurface(r, s2);
            int tw, th;
            SDL_QueryTexture(t2, NULL, NULL, &tw, &th);

            SDL_Rect d1 = { (w - tw) / 2 + 2, 40 + 2, tw, th };
            SDL_Rect d2 = { (w - tw) / 2,     40,     tw, th };

            if (t1) SDL_RenderCopy(r, t1, NULL, &d1);
            if (t2) SDL_RenderCopy(r, t2, NULL, &d2);

            if (t1) SDL_DestroyTexture(t1);
            if (t2) SDL_DestroyTexture(t2);
        }
        if (s1) SDL_FreeSurface(s1);
        if (s2) SDL_FreeSurface(s2);
        */
    }

    // 3) 서브 타이틀
    /*
    if (G_FontMain)
    {
        SDL_Color subC = {210, 230, 220, 255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(G_FontMain, "날씨 기반 반려식물 시뮬레이터", subC);
        if (s)
        {
            SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
            int w, h;
            SDL_GetRendererOutputSize(r, &w, &h);
            int tw, th;
            SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = {(w - tw) / 2, 40 + 48 + 12, tw, th};
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
    */

    // 4) 버튼
    ui_button_render(r, G_FontMain, &s_buttons[0], NULL);
    ui_button_render(r, G_FontMain, &s_buttons[1], NULL);
    ui_button_render(r, G_FontMain, &s_buttons[2], NULL);
    ui_button_render(r, G_FontMain, &s_buttons[3], NULL);
    ui_button_render(r, G_FontMain, &s_buttons[4], NULL);
    ui_button_render(r, G_FontMain, &s_buttons[5], NULL);
}

static void cleanup(void)
{
    if (s_bgAtlas)
    {
        SDL_DestroyTexture(s_bgAtlas);
        s_bgAtlas = NULL;
        s_bgFrameCount = 0;
        s_bgFrameIndex = 0;
        s_bgFrameElapsedMs = 0.f;
    }
    destroy_frame_textures();
    s_bgFrameCount = 0;
    s_bgFrameIndex = 0;
    s_bgFrameElapsedMs = 0.f;
    s_bgFramesUseAtlas = false;
    if (s_titleFont)
    {
        TTF_CloseFont(s_titleFont);
        s_titleFont = NULL;
    }
}

static Scene SCENE_OBJ = {init, handle, update, render, cleanup, "MainMenu"};
Scene *scene_mainmenu_object(void) { return &SCENE_OBJ; }
