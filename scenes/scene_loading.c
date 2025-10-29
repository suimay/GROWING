#include "../include/loading.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include <SDL2/SDL_image.h>
#include <parson.h>


LoadingData LD;                 // 전역 정의 (단 한 번)

static SDL_Texture* s_spinner = NULL;

static UIProgressBar g_LoadingBar;

typedef struct {
    SDL_Rect rect;      // 시트 내 프레임 영역
    int      duration;  // ms 단위
} LFrame;

static SDL_Texture* s_animAtlas = NULL; // 예: assets/images/loading_sheet.png
static LFrame       s_animFrames[128];
static int          s_animCount = 0;
static int          s_animIndex = 0;
static float        s_animElapsed = 0.f;

static int   s_waiting_after_done = 0;   // 완료 후 대기중인지
static float s_wait_timer = 0.0f;         // 누적 시간
static const float S_WAIT_DURATION = 3.0f; // ✅ n초 대기


static int load_loading_anim(const char* sheetPath, const char* jsonPath) {
    // 시트 텍스처
    if (s_animAtlas) { SDL_DestroyTexture(s_animAtlas); s_animAtlas = NULL; }
    s_animAtlas = IMG_LoadTexture(G_Renderer, sheetPath);
    if (!s_animAtlas) {
        SDL_Log("[LOADING ANIM] sheet load fail: %s", IMG_GetError());
        return 0;
    }
    int atlasW = 0, atlasH = 0; SDL_QueryTexture(s_animAtlas, NULL, NULL, &atlasW, &atlasH);

    // JSON 파싱
    JSON_Value* root = json_parse_file(jsonPath);
    if (!root) { SDL_Log("[LOADING ANIM] json parse fail"); return 0; }
    JSON_Object* robj = json_value_get_object(root);
    JSON_Object* framesObj = json_object_get_object(robj, "frames");
    JSON_Array* framesArr = json_object_get_array(robj, "frames");

    s_animCount = 0; s_animIndex = 0; s_animElapsed = 0.f;
    if (framesObj) {
        size_t n = json_object_get_count(framesObj);
        for (size_t i = 0;i < n && s_animCount < SDL_arraysize(s_animFrames);++i) {
            const char* key = json_object_get_name(framesObj, i);
            JSON_Object* f = json_object_get_object(framesObj, key);
            if (!f) continue;
            JSON_Object* r = json_object_get_object(f, "frame"); if (!r) continue;

            LFrame fr;
            fr.rect.x = (int)json_object_get_number(r, "x");
            fr.rect.y = (int)json_object_get_number(r, "y");
            fr.rect.w = (int)json_object_get_number(r, "w");
            fr.rect.h = (int)json_object_get_number(r, "h");
            fr.duration = (int)json_object_get_number(f, "duration");
            if (fr.duration <= 0) fr.duration = 100;

            // 범위 체크
            if (fr.rect.w <= 0 || fr.rect.h <= 0) continue;
            if (fr.rect.x<0 || fr.rect.y<0 ||
                fr.rect.x + fr.rect.w>atlasW || fr.rect.y + fr.rect.h>atlasH) continue;

            s_animFrames[s_animCount++] = fr;
        }
    }
    else if (framesArr) {
        size_t n = json_array_get_count(framesArr);
        for (size_t i = 0;i < n && s_animCount < SDL_arraysize(s_animFrames);++i) {
            JSON_Object* f = json_array_get_object(framesArr, i);
            if (!f) continue;
            JSON_Object* r = json_object_get_object(f, "frame"); if (!r) continue;

            LFrame fr;
            fr.rect.x = (int)json_object_get_number(r, "x");
            fr.rect.y = (int)json_object_get_number(r, "y");
            fr.rect.w = (int)json_object_get_number(r, "w");
            fr.rect.h = (int)json_object_get_number(r, "h");
            fr.duration = (int)json_object_get_number(f, "duration");
            if (fr.duration <= 0) fr.duration = 100;

            if (fr.rect.w <= 0 || fr.rect.h <= 0) continue;
            if (fr.rect.x<0 || fr.rect.y<0 ||
                fr.rect.x + fr.rect.w>atlasW || fr.rect.y + fr.rect.h>atlasH) continue;

            s_animFrames[s_animCount++] = fr;
        }
    }
    json_value_free(root);

    SDL_Log("[LOADING ANIM] loaded frames=%d", s_animCount);
    return (s_animCount > 0);
}


static void init(void* arg) {
    (void)arg;
    LD.done = 0;
    LD.progress = 0;

    s_waiting_after_done = 0;   // ✅ 리셋
    s_wait_timer = 0.0f;        // ✅ 리셋

    if (!s_spinner)
        s_spinner = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "spinner.png");

    load_loading_anim(ASSETS_IMAGES_DIR "loading_leaf.png",ASSETS_DIR "data/loading_leaf.json");

    int w,h; SDL_GetRendererOutputSize(G_Renderer,&w,&h);
    ui_progress_init(&g_LoadingBar, (SDL_Rect){ (w-420)/2, (h/2)+60, 420, 22 });
}

static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) G_Running = 0;
}

static void update(float dt) {
    (void)dt;

    if (s_animCount > 0) {
        s_animElapsed += dt * 1000.f;
        int dur = s_animFrames[s_animIndex].duration;
        if (dur <= 0) dur = 100;
        while (s_animElapsed >= dur) {
            s_animElapsed -= dur;
            s_animIndex = (s_animIndex + 1) % s_animCount;
            dur = s_animFrames[s_animIndex].duration;
            if (dur <= 0) dur = 100;
        }
    }

    if (s_waiting_after_done) {
        s_wait_timer += dt;
        if (s_wait_timer >= S_WAIT_DURATION) {
            s_waiting_after_done = 0;
            scene_switch_fade(LD.target, 0.2f, 0.2f);
        }
        return;
    }

    if (LD.job && !LD.done) {
        float p = LD.progress;
        int finished = LD.job(LD.userdata, &p);
        LD.progress = p;
        ui_progress_set(&g_LoadingBar, p / 100.0f);  // 0~1 스케일

        if (finished) {
            LD.done = 1;
            LD.progress = 100.0f;
            ui_progress_set(&g_LoadingBar, 1.0f);

            // ✅ 완료 후 대기 시작
            s_waiting_after_done = 1;
            s_wait_timer = 0.0f;
        }
        return;
    }

    // ✅ 완료 후 5초 대기 타이머
    if (s_waiting_after_done) {
        s_wait_timer += dt;
        if (s_wait_timer >= S_WAIT_DURATION) {
            s_waiting_after_done = 0;
            // 로딩씬 → 타겟 씬으로 페이드
            scene_switch_fade(LD.target, 0.2f, 0.2f);
        }
    }
}

static void render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 14,18,26,255);
    SDL_RenderClear(r);

    int w,h; SDL_GetRendererOutputSize(r,&w,&h);

    if (s_animAtlas && s_animCount > 0) {
        const LFrame* fr = &s_animFrames[s_animIndex];
        // 스케일 배수 (원본이 작다면 키워서)
        const int scale = 3; // 원하는 배수
        SDL_Rect dst = {
            (w - fr->rect.w * scale),
            (h - fr->rect.h * scale),  // 텍스트와 겹치지 않게 살짝 위
            fr->rect.w * scale,
            fr->rect.h * scale
        };
        SDL_RenderCopy(r, s_animAtlas, &fr->rect, &dst);
    }

    if (G_FontMain) {
        SDL_Color fg = {217,54,214,255};
        char buf[64]; SDL_snprintf(buf,sizeof(buf),"Loading... %d%%",(int)LD.progress);
        SDL_Surface* s = TTF_RenderUTF8_Blended(G_FontMain, buf, fg);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r,s);
            int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th);
            SDL_Rect d = { (w-tw)/2, (h-th)/2 - 20, tw, th };
            SDL_RenderCopy(r,t,NULL,&d);
            SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }

    if (s_spinner) {
        SDL_Rect d = { w/2-32, h/2+20, 64,64 };
        SDL_RenderCopy(r, s_spinner, NULL, &d);
    }

    ui_progress_render(r, G_FontMain, &g_LoadingBar);
}

static void cleanup(void) { if (s_animAtlas) { SDL_DestroyTexture(s_animAtlas); s_animAtlas = NULL; } }

void loading_begin(SceneID target, LoadingJobFn job, void* userdata,
                   float fade_out_sec, float fade_in_sec)
{
    LD.target   = target;
    LD.job      = job;
    LD.userdata = userdata;
    LD.progress = 0;
    LD.done     = 0;

    scene_switch_fade(SCENE_LOADING, fade_out_sec, 0.2f);
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Loading" };
Scene* scene_loading_object(void) { return &SCENE_OBJ; }
