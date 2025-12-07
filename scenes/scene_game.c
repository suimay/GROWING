// scenes/scene_gameplay.c
#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include "../include/core.h"       // PlantInfo, plantdb_get
#include "../include/save.h"       // log_water, log_window
#include "../include/settings.h"   // settings_apply_audio
#include "../include/gameplay.h"
#include "../include/weather.h"
#include "../include/anim_util.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <parson.h>

LoadCtx s_loadCtx;          // 정의(여기 딱 1곳)

// -----------------------------
// 상태
extern void gameplay_ensure_bgm_loaded(void);
extern void load_background_animation(void);
extern void settings_apply_audio(void);

static int  lamp_panel = 0;
static int  s_room_temperature = 20;
static bool window_open = false;
static bool back_panel = false;
static int  exit_pressed = 0;

static int s_step = 0;  // 0부터 차례로 증가

// -----------------------------
static const PlantInfo* s_plant = NULL;
static PlantStatus s_status;

static UIButton s_btnBack;
static UIButton s_btnWater;
static UIButton s_btnWindow;
static UIButton s_btnnobug;
static UIButton s_btnnogom;
static UIButton s_btnifcold;
static UIButton s_btnifhot;
static UIButton s_btnlamp;
static UIButton s_btnbiryo;
static UIButton s_btnlpup;
static UIButton s_btnlpdown;
static UIButton s_btnexit;

static int  s_waterCount = 0;
static bool s_windowOpen = false;
static int  s_light_level = 0;

static SDL_Texture* s_bgTexture = NULL;   // fallback 배경(옵션)
static SDL_Texture* s_backIcon = NULL;   // ← 버튼 아이콘(옵션)
static SDL_Texture* s_water = NULL;
static SDL_Texture* s_window = NULL;
static SDL_Texture* s_nobug = NULL;
static SDL_Texture* s_nogom = NULL;
static SDL_Texture* s_ifhot = NULL;
static SDL_Texture* s_ifcold = NULL;
static SDL_Texture* s_lamp = NULL;
static SDL_Texture* s_biryo = NULL;
static SDL_Texture* s_lamp_hotbar = NULL;
static SDL_Texture* s_lamp_levelup = NULL;
static SDL_Texture* s_lamp_leveldown = NULL;
static SDL_Texture* s_exit = NULL;

///// 식물 경험치
static int   s_plantLevel = 3;
static float s_plantExp = 0.f;

////////////////////////////////////////// 식물 텍스쳐 (예시)
static SDL_Texture* s_monstera = NULL;

/////////////////////////////////////// 날씨!
static WeatherInfo g_weather;
static Uint32 g_last_weather_check = 0;
static const Uint32 WEATHER_INTERVAL_MS = 60 * 1000 * 5; // 5분

static SDL_Texture* s_weatherClouds = NULL;
static SDL_Texture* s_weatherRain = NULL;
static SDL_Texture* s_weatherSnow = NULL;

// 시간대(낮/밤/일출몰) 캐시
static TimeOfDay g_timeOfDay = TIMEOFDAY_DAY;
static Uint32   g_last_tod_check = 0;
static const Uint32 TOD_INTERVAL_MS = 10 * 1000; // 10초마다 갱신

static int s_eventtype = 0;

static bool s_weather_event_over = false;

////////////////////////////////////////////////// 게임 로직
float optM;
bool moisture_ok;
bool temp_ok;
static float s_room_humidity = 60.f; // 0~100
bool humidity_ok;
static bool s_hasBug = false;
static bool s_hasMold = false;
bool nutrition_ok ; 

static int   s_activeWeatherEvent = 0;
static float s_weatherEventTimer = 0.f;

float cooltime = 20.f;

static WeatherTag s_originalWeatherTag = WEATHER_TAG_CLEAR;

// 액션별 경험치 기본값 🐥

#define EXP_WATER 1.0f
#define EXP_KILL_BUG 10.0f
#define EXP_REMOVE_MOLD 12.0f
#define EXP_GIVE_FERT 5.0f
#define EXP_TEMP_DOWN 3.0f
#define EXP_TEMP_UP 3.0f

// 어떤 업데이트 함수 안에서
void update_status(void)
{
    optM = s_plant->moisture_opt;

    moisture_ok = (s_status.moisture >= optM - 10.0f && s_status.moisture <= optM + 10.0f);
    temp_ok = (s_status.temp >= s_plant->temp_min && s_status.temp <= s_plant->temp_max);
    humidity_ok = (s_status.humidity >= s_plant->humidity_min && s_status.humidity <= s_plant->humidity_max);
    nutrition_ok = (s_status.nutrition >= 40.f); // 추측
}

//////////////////////////////////////////////////////////////////////////////////// 게임 로직 함수
static void update_moisture(float dt)
{
    float basePerMin = 0.f;

    switch (g_weather.tag) {
    case WEATHER_TAG_CLEAR:   basePerMin = 10.f; break;
    case WEATHER_TAG_RAIN:    basePerMin = 5.f;  break;
    case WEATHER_TAG_CLOUDY:  basePerMin = 8.f;  break;
    case WEATHER_TAG_SNOW:    basePerMin = 6.f;  break; // ★ 눈: 추측값
    default:                  basePerMin = 8.f;  break;
    }

    // 1분에 basePerMin 만큼 줄어든다고 가정 → 1초당 basePerMin/60
    float perSec = basePerMin / 60.f;
    s_status.moisture -= perSec * dt;
    update_status();
    if (s_status.moisture < 0.f) s_status.moisture = 0.f;
}

static void update_temperature(float dt)
{
    float perMin = 0.f;

    switch (g_weather.tag) {
    case WEATHER_TAG_CLEAR:  perMin = +0.5f; break;
    case WEATHER_TAG_SNOW:   perMin = -0.5f; break;
    case WEATHER_TAG_RAIN:
    case WEATHER_TAG_CLOUDY: perMin = 0.0f; break;
    default:                  perMin = 0.0f; break;
    }

    float perSec = perMin / 60.f;
    s_room_temperature += perSec * dt;
    s_status.temp = (float)s_room_temperature;

    update_status();
}

static void update_humidity(float dt)
{
    float perMin = 0.f;

    if (g_weather.tag == WEATHER_TAG_CLEAR) {
        // 맑음 → 감소
        perMin = -2.0f;
    }
    else if (g_weather.tag == WEATHER_TAG_RAIN) {
        // 비 → 창문 열면 많이 증가, 닫으면 조금 증가 (추측)
        if (window_open)
            perMin = +3.0f;
        else
            perMin = +1.0f;
    }
    else {
        // 구름/눈 → 거의 유지 (약간만 변화)
        perMin = 0.0f;
    }

    float perSec = perMin / 60.f;
    s_room_humidity += perSec * dt;

    if (s_room_humidity < 0.f)   s_room_humidity = 0.f;
    if (s_room_humidity > 100.f) s_room_humidity = 100.f;

    s_status.humidity = s_room_humidity;

    update_status();
}

static void update_random_events(float dt)
{
    // dt기반으로도 만들 수 있지만, 간단하게 매 프레임 작은 확률
    float bugChancePerSec = 0.0005f; // 추측 //0.0005f
    float moldChancePerSec = 0.0005f; // 추측

    float bugProbThisFrame = bugChancePerSec * dt;
    float moldProbThisFrame = moldChancePerSec * dt;

    if (!s_hasBug) {
        float r = (float)rand() / (float)RAND_MAX;
        if (r < bugProbThisFrame) {
            s_hasBug = true;
            SDL_Log("[EVENT] Bug appeared!");
            
        }
    }

    if (!s_hasMold) {
        float r = (float)rand() / (float)RAND_MAX;
        if (r < moldProbThisFrame) {
            s_hasMold = true;
            SDL_Log("[EVENT] Mold appeared!");
        }
    }

    update_status();
}

static void update_happiness(float dt)
{
    float deltaPerMin = 0.f;

    if (s_hasBug)  deltaPerMin -= 3.f; // 추측
    if (s_hasMold) deltaPerMin -= 4.f; // 추측

    float perSec = deltaPerMin / 60.f;
    s_status.happiness += perSec * dt;

    if (s_status.happiness < 0.f)   s_status.happiness = 0.f;
    if (s_status.happiness > 100.f) s_status.happiness = 100.f;

    update_status();
}

static void update_nutrition(float dt)
{
    float perMin = -2.0f;           // 추측
    float perSec = perMin / 60.f;

    s_status.nutrition += perSec * dt;
    if (s_status.nutrition < 0.f)   s_status.nutrition = 0.f;
    if (s_status.nutrition > 100.f) s_status.nutrition = 100.f;

    update_status();
}

void start_weather_event(WeatherTag eventTag, float duration)
{
    if (s_activeWeatherEvent) return;  // 이미 이벤트 중이면 무시

    s_activeWeatherEvent = 1;
    s_weatherEventTimer = duration;

    SDL_Log("[WEATHER EVENT] BEFORE START: g_weather.tag=%d", (int)g_weather.tag);

    // ★ 여기서 '그 시점의 실제 날씨'를 저장
    s_originalWeatherTag = g_weather.tag;

    // 그리고 이벤트용 날씨로 바꿈
    g_weather.tag = eventTag;

    SDL_Log("[WEATHER EVENT] start: base=%d event=%d", (int)s_originalWeatherTag, (int)eventTag);
}

static void update_weather_event(float dt )
{
    
   
    if (s_activeWeatherEvent == 0 ) {
        // 이벤트 없음 → 아주 낮은 확률로 발생 (추측)
        float chancePerSec = 0.0001f; // 0.0001f
        float p = chancePerSec * dt;
        float r = (float)rand() / (float)RAND_MAX;

        int weather = rand() % 10;
        
        //printf("%d < %d %d \n",r,p, weather);
        if (r < p) {
            // 현재 실제 날씨랑 랜덤 event 날씨를 하나 선택 (추측)
            //s_activeWeatherEvent = g_weather.tag; // 일단 동일로
            int w = rand() % 10;
            WeatherTag eventTag;

            if (w < 2) eventTag = WEATHER_TAG_RAIN; // 3
            else if (w < 4) eventTag = WEATHER_TAG_CLOUDY; // 2
            else if (w < 6) eventTag = WEATHER_TAG_SNOW; // 4
            else            eventTag = WEATHER_TAG_CLEAR; // 1

            start_weather_event(eventTag, 60.f);

            // 1~2분 사이 지속 (추측)
            float durMin = 1.f + ((float)rand() / RAND_MAX) * 1.f;
            s_weatherEventTimer = durMin * 20.f; //durMin * 60.f; 
            
        }
    }
    else {
        // 진행 중
        s_weatherEventTimer -= dt;
        if (s_weatherEventTimer <= 0.f) {
            SDL_Log("[WEATHER EVENT] end, restore=%d", (int)s_originalWeatherTag);
            g_weather.tag = s_originalWeatherTag;  // ★ 원래 날씨 복원
            s_activeWeatherEvent = 0;
            cooltime = 20.f;
        }
    }

    update_status();

}

static float get_exp_multiplier(void)
{
    // 현재 실제 날씨와 이벤트 날씨가 같으면 1.5배 (추측)
    if (s_activeWeatherEvent != WEATHER_TAG_CLEAR &&
        s_activeWeatherEvent == g_weather.tag) {
        return 1.5f;
    }
    return 1.0f;
}

static void add_exp(float baseExp)
{
    if (baseExp <= 0.f)
        return;

    float mul = get_exp_multiplier();
    s_plantExp += baseExp * mul;

    while (s_plantExp >= 100.f)
    {
        s_plantExp -= 100.f;
        s_plantLevel++;
        SDL_Log("[GAME] plant level up! level = %d", s_plantLevel);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static SDL_Texture* s_pot = NULL;



//////////////////////////////////// 시간대 배경
static SDL_Texture* s_bgDay = NULL;
static SDL_Texture* s_bgNight = NULL;
static SDL_Texture* s_bgSunrise = NULL;  // 일출/일몰 공용
static bool background_change = false;

static int   g_tod_fade_active = 0;      // 0: 없음, 1: 진행 중
static float g_tod_fade_t = 0.0f;   // 진행 시간(sec)
static const float TOD_FADE_TIME = 2.0f;   // 총 페이드 시간

static TimeOfDay g_tod_before = TIMEOFDAY_DAY;  // 전환 전 시간대
static TimeOfDay g_tod_after = TIMEOFDAY_DAY;  // 전환 후 시간대

// -----------------------------
// 인게임 BGM (랜덤 재생)
// -----------------------------

void update_weather_if_needed(void)
{
    Uint32 now = SDL_GetTicks();

    if (now - g_last_weather_check >= WEATHER_INTERVAL_MS) {
        g_last_weather_check = now;

        system("python assets/weather_update.py");

        if (weather_load(&g_weather)) {
            weather_compute_sun_secs(&g_weather);
        }

        printf("[날씨 갱신]\n");
        printf("TAG=%d  TEMP=%.2f  HUM=%d\n", g_weather.tag, g_weather.temp, g_weather.humidity);
        printf("SUNRISE=%s  SUNSET=%s\n", g_weather.sunrise, g_weather.sunset);
    }
}

static void render_time_tint(SDL_Renderer* r, TimeOfDay tod, int w, int h)
{
    Uint8 R = 0, G = 0, B = 0, A = 0;

    switch (tod) {
    case TIMEOFDAY_DAY:
        R = 255; G = 245; B = 220; A = 20;
        break;
    case TIMEOFDAY_SUNRISE:
    case TIMEOFDAY_SUNSET:
        R = 255; G = 170; B = 120; A = 70;
        break;
    case TIMEOFDAY_NIGHT:
        R = 10;  G = 20;  B = 40;  A = 120;
        break;
    default:
        A = 0;
        break;
    }

    if (A == 0) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);

    SDL_Rect full = { 0, 0, w, h };
    SDL_RenderFillRect(r, &full);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

int gameplay_loading_job(void* u, float* out_p)
{
    LoadCtx* c = (LoadCtx*)u;
    if (!c || !out_p) return 1;

    Uint32 now = SDL_GetTicks();

    switch (c->step) {
    case 0:
        *out_p = 5.f;
        c->t0 = now;
        c->step = 1;
        return 0;

    case 1:
        if (now - c->t0 < 120) { *out_p = 35.f; return 0; }
        load_background_animation();
        *out_p = 75.f;
        c->step = 2;
        return 0;

    case 2:
        gameplay_ensure_bgm_loaded();
        *out_p = 95.f;
        c->step = 99;
        return 0;

    case 99:
        *out_p = 100.f;
        return 1;
    }
    return 0;
}

// 시간 문자열
static void get_current_time_string(char* out, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(out, size, "%H:%M:%S", t);
}
static void get_current_day_string(char* out, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(out, size, "%Y-%m-%d", t);
}

// -----------------------------
// BGM
// -----------------------------
static const char* kGameBGMs[] = {
    ASSETS_SOUNDS_DIR "PLAYSCENEBGM1.wav",
    ASSETS_SOUNDS_DIR "Snowy_winter.wav",
    ASSETS_SOUNDS_DIR "In_the_fairytale.wav",
    ASSETS_SOUNDS_DIR "To_School_at_Sunrise.wav",
    ASSETS_SOUNDS_DIR "Living_in_the_grass.wav",
};
static Mix_Music* s_gameBGMs[SDL_arraysize(kGameBGMs)] = { 0 };
static int s_bgmLoaded = 0;
static int s_lastBgm = -1;

// -----------------------------
// 배경 애니메이션(아틀라스)
// -----------------------------
typedef struct {
    SDL_Rect rect;
    int      duration_ms;
    SDL_Texture* texture; // fallback 시 사용
} AnimFrame;

static SDL_Texture* s_bgAtlas = NULL;
static SDL_Texture* s_room = NULL;
static SDL_Texture* s_room_open = NULL;
static AnimFrame    s_bgFrames[128];
static int          s_bgFrameCount = 0;
static int          s_bgFrameIndex = 0;
static float        s_bgFrameElapsedMs = 0.f;
static bool         s_bgFramesUseAtlas = false;

///////////////////////////////////////////////////////
// 이벤트 애니메이션
///////////////////////////////////////////////////////

#define BUG_IDLE_FRAME_W 480
#define BUG_IDLE_FRAME_H 270
#define BUG_IDLE_FRAMES 4

#define MOLD_IDLE_FRAME_W 480
#define MOLD_IDLE_FRAME_H 270
#define MOLD_IDLE_FRAMES 4

#define BUG_SPRAY_FRAME_W 480
#define BUG_SPRAY_FRAME_H 270
#define BUG_SPRAY_FRAMES 6

#define MOLD_SPRAY_FRAME_W 480
#define MOLD_SPRAY_FRAME_H 270
#define MOLD_SPRAY_FRAMES 6

#define IDLE_FPS 6.0f
#define SPRAY_FPS 12.0f


typedef struct
{
    float timer;
    int currentFrame;
    int totalFrames;
    float frameDuration;
} IdleAnim;

typedef struct
{
    bool active;
    float timer;
    int currentFrame;
    int totalFrames;
    float frameDuration;
} SprayAnim;

static SDL_Texture* s_texBugIdle = NULL;
static SDL_Texture* s_texMoldIdle = NULL;
static SDL_Texture* s_texBugSpray = NULL;
static SDL_Texture* s_texMoldSpray = NULL;

static IdleAnim s_bugIdleAnim = { 0 };
static IdleAnim s_moldIdleAnim = { 0 };
static SprayAnim s_bugSprayAnim = { 0 };
static SprayAnim s_moldSprayAnim = { 0 };


typedef struct {
    SDL_Rect rect;
    int      duration_ms;
} EventFrame;

static SDL_Texture* s_eventAtlas = NULL;
static EventFrame   s_eventFrames[32];
static int          s_eventFrameCount = 0;
static int          s_eventFrameIndex = 0;
static float        s_eventFrameElapsedMs = 0.f;

static SDL_Rect s_eventDstRect = { 0, 0, 1920, 1080 }; // 전체 화면

static int   s_eventPlaying = 0;  // 0 = 안 나옴, 1 = 재생 중
static int   s_eventOneShot = 1;  // 1이면 한 번 재생 후 꺼짐

// -----------------------------
// 유틸
// -----------------------------
static void draw_text(SDL_Renderer* r, SDL_Color color, int x, int y, const char* fmt, ...)
{
    if (!G_FontMain || !fmt) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_Surface* surf = TTF_RenderUTF8_Blended(G_FontMain, buf, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_text2(SDL_Renderer* r, SDL_Color color, int x, int y, int size_x, int size_y, const char* fmt, ...)
{
    if (!G_FontMain || !fmt) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_Surface* surf = TTF_RenderUTF8_Blended(G_FontMain, buf, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, size_x, size_y };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void destroy_frame_textures(void)
{
    for (int i = 0; i < (int)SDL_arraysize(s_bgFrames); ++i) {
        if (s_bgFrames[i].texture) {
            SDL_DestroyTexture(s_bgFrames[i].texture);
            s_bgFrames[i].texture = NULL;
        }
    }
}

static void gameplay_ensure_bgm_loaded(void)
{
    if (s_bgmLoaded) return;

    for (int i = 0; i < (int)SDL_arraysize(kGameBGMs); ++i) {
        s_gameBGMs[i] = Mix_LoadMUS(kGameBGMs[i]);
        if (!s_gameBGMs[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[GAMEPLAY BGM] Load fail %s : %s", kGameBGMs[i], Mix_GetError());
        }
        else {
            SDL_Log("[GAMEPLAY BGM] loaded: %s", kGameBGMs[i]);
        }
    }
    s_bgmLoaded = 1;

    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
}

static int pick_random_index(void)
{
    int n = (int)SDL_arraysize(kGameBGMs);
    int tries = 10;
    while (tries--) {
        int r = rand() % n;
        if (r != s_lastBgm) return r;
    }
    return (s_lastBgm + 1) % n;
}

static void SDLCALL gameplay_on_music_finished(void)
{
    int idx = pick_random_index();
    s_lastBgm = idx;
    if (s_gameBGMs[idx]) {
        if (Mix_FadeInMusic(s_gameBGMs[idx], /*loops=*/1, /*ms=*/200) < 0) {
            SDL_Log("[GAMEPLAY BGM] FadeIn error: %s", Mix_GetError());
        }
    }
}

// -----------------------------
// 이벤트 애니 helper
// -----------------------------
static void destroy_event_frames(void)
{
    if (s_eventAtlas) {
        SDL_DestroyTexture(s_eventAtlas);
        s_eventAtlas = NULL;
    }
    s_eventFrameCount = 0;
    s_eventFrameIndex = 0;
    s_eventFrameElapsedMs = 0.f;
}

static void load_event_animation(void)
{
    destroy_event_frames();

    const char* pngPath = NULL;
    const char* jsonPath = NULL;

    switch (s_eventtype) {
    case 1:
        pngPath = ASSETS_IMAGES_DIR "event_water.png";
        jsonPath = ASSETS_DIR       "data/event_water.json";
        break;
    case 2:
        pngPath = ASSETS_IMAGES_DIR "event_bugs.png";
        jsonPath = ASSETS_DIR       "data/event_bugs.json";
        break;
    case 3:
        pngPath = ASSETS_IMAGES_DIR "event_gompang.png";
        jsonPath = ASSETS_DIR       "data/event_gompang.json";
        break;
    case 4:
        pngPath = ASSETS_IMAGES_DIR "event_tempUp.png";
        jsonPath = ASSETS_DIR       "data/event_tempUp.json";
        break;
    case 5:
        pngPath = ASSETS_IMAGES_DIR "event_tempDown.png";
        jsonPath = ASSETS_DIR       "data/event_tempDown.json";
        break;
    case 6:
        pngPath = ASSETS_IMAGES_DIR "event_food.png";
        jsonPath = ASSETS_DIR       "data/event_food.json";
        break;
    default:
        SDL_Log("[event] unknown type %d", s_eventtype);
        return;
    }

    s_eventAtlas = IMG_LoadTexture(G_Renderer, pngPath);
    if (!s_eventAtlas) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[event] IMG_LoadTexture fail %s : %s",
            pngPath, IMG_GetError());
        return;
    }

    int atlasW = 0, atlasH = 0;
    SDL_QueryTexture(s_eventAtlas, NULL, NULL, &atlasW, &atlasH);

    JSON_Value* root = json_parse_file(jsonPath);
    if (!root) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[event] json parse fail %s", jsonPath);
        destroy_event_frames();
        return;
    }

    JSON_Object* robj = json_value_get_object(root);
    JSON_Object* framesObj = json_object_get_object(robj, "frames");
    if (!framesObj) {
        json_value_free(root);
        destroy_event_frames();
        return;
    }

    size_t count = json_object_get_count(framesObj);
    for (size_t i = 0; i < count && s_eventFrameCount < (int)SDL_arraysize(s_eventFrames); ++i) {
        const char* key = json_object_get_name(framesObj, i);
        JSON_Object* fobj = json_object_get_object(framesObj, key);
        if (!fobj) continue;

        JSON_Object* rect = json_object_get_object(fobj, "frame");
        if (!rect) continue;

        EventFrame fr;
        fr.rect.x = (int)json_object_get_number(rect, "x");
        fr.rect.y = (int)json_object_get_number(rect, "y");
        fr.rect.w = (int)json_object_get_number(rect, "w");
        fr.rect.h = (int)json_object_get_number(rect, "h");
        fr.duration_ms = (int)json_object_get_number(fobj, "duration");
        if (fr.duration_ms <= 0) fr.duration_ms = 100;

        s_eventFrames[s_eventFrameCount++] = fr;
    }

    json_value_free(root);

    SDL_Log("[event] loaded %d frames (atlas=%p, size=%dx%d)",
        s_eventFrameCount, (void*)s_eventAtlas, atlasW, atlasH);
}

static void event_anim_start(void)
{
    if (s_eventFrameCount <= 0 || !s_eventAtlas) {
        SDL_Log("[event] cannot start: no frames or atlas");
        return;
    }

    if (s_eventtype == 2) {
        s_eventPlaying = 0;
        s_eventFrameIndex = 0;
        s_eventFrameElapsedMs = 0.f;
    }
    else if (s_eventtype == 3){
        s_eventPlaying = 0;
        s_eventFrameIndex = 0;
        s_eventFrameElapsedMs = 0.f;
    }
    else {
        s_eventPlaying = 1;
        s_eventFrameIndex = 0;
        s_eventFrameElapsedMs = 0.f;
    }

   
}

static void event_anim_update(float dt)
{
    if (!s_eventPlaying) return;
    if (s_eventFrameCount <= 0) return;

    s_eventFrameElapsedMs += dt * 1000.f;

    int duration = s_eventFrames[s_eventFrameIndex].duration_ms;
    if (duration <= 0) duration = 100;

    while (s_eventFrameElapsedMs >= duration) {
        s_eventFrameElapsedMs -= duration;
        s_eventFrameIndex++;

        if (s_eventFrameIndex >= s_eventFrameCount) {
            if (s_eventOneShot) {
                s_eventPlaying = 0;
                s_eventFrameIndex = s_eventFrameCount - 1;
                break;
            }
            else {
                s_eventFrameIndex = 0;
            }
        }

        duration = s_eventFrames[s_eventFrameIndex].duration_ms;
        if (duration <= 0) duration = 100;
    }
}

static void event_anim_render(SDL_Renderer* r)
{
    if (!s_eventPlaying) return;
    if (!s_eventAtlas)   return;
    if (s_eventFrameCount <= 0) return;

    EventFrame* fr = &s_eventFrames[s_eventFrameIndex];
    SDL_Rect src = fr->rect;

    SDL_RenderCopy(r, s_eventAtlas, &src, &s_eventDstRect);
}

// 타입 지정 + 로드 + 시작 한 번에
static void event_play(int type)
{
    s_eventtype = type;
    load_event_animation();
    event_anim_start();
}

static void update_idle_anim(IdleAnim* a, float dt)
{
    if (!a || a->totalFrames <= 1)
        return;
    a->timer += dt;
    if (a->timer >= a->frameDuration)
    {
        a->timer -= a->frameDuration;
        a->currentFrame = (a->currentFrame + 1) % a->totalFrames;
    }
}

static void update_spray_anim(SprayAnim* a, float dt)
{
    if (!a || !a->active)
        return;
    a->timer += dt;
    while (a->timer >= a->frameDuration)
    {
        a->timer -= a->frameDuration;
        a->currentFrame++;
        if (a->currentFrame >= a->totalFrames)
        {
            a->active = false;
            break;
        }
    }
}

static void update_spray_anims(float dt)
{
    update_idle_anim(&s_bugIdleAnim, dt);
    update_idle_anim(&s_moldIdleAnim, dt);
    update_spray_anim(&s_bugSprayAnim, dt);
    update_spray_anim(&s_moldSprayAnim, dt);
}

static void render_idle_with_frames(SDL_Renderer* r, SDL_Texture* tex, const IdleAnim* a,
    int frameW, int frameH, int totalFrames, int x, int y)
{
    if (!tex)
        return;
    int frameCount = (totalFrames > 0) ? totalFrames : 1;
    int frameIdx = (a && frameCount > 1) ? a->currentFrame % frameCount : 0;
    SDL_Rect src = { frameIdx * frameW, 0, frameW, frameH };
    SDL_Rect dst = { x, y, frameW, frameH };
    SDL_RenderCopy(r, tex, &src, &dst);
}

static void render_spray(SDL_Renderer* r, SDL_Texture* tex, const SprayAnim* a,
    int frameW, int frameH, int totalFrames, int x, int y)
{
    if (!tex || !a || !a->active)
        return;
    int frameCount = (totalFrames > 0) ? totalFrames : 1;
    int frameIdx = (a->currentFrame < frameCount) ? a->currentFrame : frameCount - 1;
    SDL_Rect src = { frameIdx * frameW, 0, frameW, frameH };
    SDL_Rect dst = { x, y, frameW, frameH };
    SDL_RenderCopy(r, tex, &src, &dst);
}

static void render_bug_mold(SDL_Renderer* r, int screenW, int screenH)
{
    // 화분 기준 대략적인 위치 (조정해야함.. 싹싹🙏)
    int baseY = screenH - 500;
    if (s_hasBug)
    {
        int x = screenW / 2 - 300;
        render_idle_with_frames(r, s_texBugIdle, &s_bugIdleAnim,
            BUG_IDLE_FRAME_W, BUG_IDLE_FRAME_H, BUG_IDLE_FRAMES,
            x, baseY);
    }
    if (s_hasMold)
    {
        int x = screenW / 2 - 200;
        render_idle_with_frames(r, s_texMoldIdle, &s_moldIdleAnim,
            MOLD_IDLE_FRAME_W, MOLD_IDLE_FRAME_H, MOLD_IDLE_FRAMES,
            x, baseY);
    }
}

static void render_spray_anims(SDL_Renderer* r, int screenW, int screenH)
{
    int baseY = screenH - 500;
    render_spray(r, s_texBugSpray, &s_bugSprayAnim,
        BUG_SPRAY_FRAME_W, BUG_SPRAY_FRAME_H, BUG_SPRAY_FRAMES,
        screenW / 2 - 300, baseY);
    render_spray(r, s_texMoldSpray, &s_moldSprayAnim,
        MOLD_SPRAY_FRAME_W, MOLD_SPRAY_FRAME_H, MOLD_SPRAY_FRAMES,
        screenW / 2 - 200, baseY);
}


// -----------------------------
// 버튼 콜백
// -----------------------------
static void on_back(void* ud)
{
    (void)ud;
    back_panel = !back_panel;
}

static void on_exit(void* ud)
{
    (void)ud;
    scene_switch_fade(SCENE_MAINMENU, 0.3f, 0.3f);

    if (s_exit) { SDL_DestroyTexture(s_exit); s_exit = NULL; }
    back_panel = false;
}

static void on_water(void* ud)
{
    (void)ud;
    s_waterCount++;
    if (s_plant) log_water(s_plant->id, 120);

    event_play(1);      // 물 이벤트
    

    float mul = get_exp_multiplier();
    s_plantExp += 1.f * mul;
    
    add_exp(EXP_WATER); //물 경험치 추가🐥

    s_status.moisture += 10.f;

    if (s_status.moisture > 100.f)
        s_status.moisture = 100.f;

    
}

static void on_window(void* ud)
{
    (void)ud;
    s_windowOpen = !s_windowOpen;

    ui_button_init(&s_btnWindow, s_btnWindow.r, "");
    ui_button_set_callback(&s_btnWindow, on_window, NULL);
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);

    window_open = !window_open;

    SDL_Log(s_windowOpen ? "[GAME] window open." : "[GAME] window close.");
    if (s_plant) log_window(s_plant->id, s_windowOpen);
}

static void on_nobug(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnnobug, G_SFX_Click, NULL);
    event_play(2);

    if (!s_hasBug)
    {
        SDL_Log("[EVENT] no bug to kill");
        return;
    }

    // 2) 분무기/이펙트 애니메이션 시작 (스프레이 전용)
    
    s_bugSprayAnim.active = true;
    s_bugSprayAnim.timer = 0.f;
    s_bugSprayAnim.currentFrame = 0;
    

    // 3) 실제 상태 변화 (즉시 제거, 시각 효과는 계속 재생)
    s_hasBug = false;
    s_status.happiness += 10.f;
    if (s_status.happiness > 100.f)
        s_status.happiness = 100.f;

    // 4) 경험치 + 레벨업 (킬 성공시에만)
    add_exp(EXP_KILL_BUG);

    SDL_Log("[EVENT] kill bug");
}

static void on_nogom(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnnogom, G_SFX_Click, NULL);
    event_play(3);

    if (!s_hasMold)
    {
        SDL_Log("[EVENT] no mold to remove");
        return;
    }

    // 2) 분무기/이펙트 애니 시작 (스프레이 전용)
    
    s_moldSprayAnim.active = true;
    s_moldSprayAnim.timer = 0.f;
    s_moldSprayAnim.currentFrame = 0;
    

    // 3) 곰팡이 제거 + 행복도 (즉시 플래그 내림, 애니는 계속 재생)
    s_hasMold = false;
    s_status.happiness += 10.f;
    if (s_status.happiness > 100.f)
        s_status.happiness = 100.f;

    // 4) 경험치 + 레벨업
    add_exp(EXP_REMOVE_MOLD);

    SDL_Log("[EVENT] remove mold");
}

static void on_ifhot(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnifhot, G_SFX_Click, NULL);
    event_play(5);

    float mul = get_exp_multiplier();
    s_plantExp += 1.f * mul;

    add_exp(EXP_TEMP_DOWN);
    SDL_Log("temperature down");
    s_room_temperature--;
    s_status.temp = (float)s_room_temperature;
}

static void on_ifcold(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnifcold, G_SFX_Click, NULL);
    event_play(4);

    float mul = get_exp_multiplier();
    s_plantExp += 1.f * mul;

    add_exp(EXP_TEMP_UP);
    SDL_Log("temperature up");
    s_room_temperature++;
    s_status.temp = (float)s_room_temperature;
}

static void on_biryo(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnbiryo, G_SFX_Click, NULL);
    event_play(6);

    s_status.nutrition += 20.f; // 추측
    if (s_status.nutrition > 100.f) s_status.nutrition = 100.f;

    float mul = get_exp_multiplier();
    s_plantExp += 1.f * mul;

    add_exp(EXP_GIVE_FERT);

    SDL_Log("give nutrients");
}

static void update_lamp_panel_buttons(void)
{
    int active = (lamp_panel == 1);
    s_btnlpup.enabled = active;
    s_btnlpdown.enabled = active;
}

static void on_lamp(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnlamp, G_SFX_Click, NULL);

    lamp_panel = !lamp_panel;
    SDL_Log(lamp_panel ? "[GAME] light panel on." : "[GAME] light panel off.");
    update_lamp_panel_buttons();
}

static void on_lamp_up(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnlpup, G_SFX_Click, NULL);
    if (s_light_level >= 0 && s_light_level < 2) s_light_level++;
    SDL_Log("[GAME] light_level %d.", s_light_level);
}

static void on_lamp_down(void* ud)
{
    (void)ud;
    ui_button_set_sfx(&s_btnlpdown, G_SFX_Click, NULL);
    if (s_light_level > 0 && s_light_level <= 2) s_light_level--;
    SDL_Log("[GAME] light_level %d.", s_light_level);
}

// -----------------------------
// 레이아웃
// -----------------------------
static void layout(void)
{
    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);

    const int margin = 40;
    const int backSize = 72;
    const int actionWidth = 70;
    const int actionHeight = 70;
    const int actionGap = 24;

    ui_button_init(&s_btnBack, (SDL_Rect) { margin - 15, margin - 15, backSize - 20, backSize - 20 }, "");
    ui_button_init(&s_btnWater, (SDL_Rect) { margin, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnWindow, (SDL_Rect) { margin + actionWidth + actionGap, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnnobug, (SDL_Rect) { margin + (actionWidth + actionGap) * 2, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnnogom, (SDL_Rect) { margin + (actionWidth + actionGap) * 3, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnifcold, (SDL_Rect) { margin + (actionWidth + actionGap) * 4, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnifhot, (SDL_Rect) { margin + (actionWidth + actionGap) * 5, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnlamp, (SDL_Rect) { margin + (actionWidth + actionGap) * 6, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnbiryo, (SDL_Rect) { margin + (actionWidth + actionGap) * 7, h - actionHeight - margin, actionWidth, actionHeight }, "");
    ui_button_init(&s_btnlpup, (SDL_Rect) { 560 + 10, 870 + 15, 70, 70 }, "");
    ui_button_init(&s_btnlpdown, (SDL_Rect) { 560 + 10 + 70, 870 + 15, 70, 70 }, "");
    ui_button_init(&s_btnexit, (SDL_Rect) { w / 2 - 35, h / 2 + 70, 70, 70 }, "");

    ui_button_set_callback(&s_btnBack, on_back, NULL);
    ui_button_set_callback(&s_btnWater, on_water, NULL);
    ui_button_set_callback(&s_btnWindow, on_window, NULL);
    ui_button_set_callback(&s_btnlamp, on_lamp, NULL);
    ui_button_set_callback(&s_btnlpup, on_lamp_up, NULL);
    ui_button_set_callback(&s_btnlpdown, on_lamp_down, NULL);
    ui_button_set_callback(&s_btnnobug, on_nobug, NULL);
    ui_button_set_callback(&s_btnnogom, on_nogom, NULL);
    ui_button_set_callback(&s_btnifhot, on_ifhot, NULL);
    ui_button_set_callback(&s_btnifcold, on_ifcold, NULL);
    ui_button_set_callback(&s_btnbiryo, on_biryo, NULL);
    ui_button_set_callback(&s_btnexit, on_exit, NULL);

    ui_button_set_sfx(&s_btnBack, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWater, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnWindow, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnnobug, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnnogom, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnifcold, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnifhot, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnlamp, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnbiryo, G_SFX_Click, NULL);
    ui_button_set_sfx(&s_btnexit, G_SFX_Click, NULL);
}

// -----------------------------
// 배경 애니메이션 로드 (기존 내용 그대로 둠)
// -----------------------------
static void load_background_animation(void)
{
    destroy_frame_textures();
    s_bgFrameCount = 0;
    s_bgFrameIndex = 0;
    s_bgFrameElapsedMs = 0.f;
    s_bgFramesUseAtlas = false;

    if (s_bgAtlas) { SDL_DestroyTexture(s_bgAtlas); s_bgAtlas = NULL; }


    s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "sunset.png");
    if (!s_bgAtlas) {
        SDL_Log("[ANIM] sunset.png failed: %s", IMG_GetError());
    }

    s_room = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "room.png");
    s_room_open = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "room-2-2.png");

    JSON_Value* root = json_parse_file(ASSETS_DIR "data/sunset.json");
    if (!root) { SDL_Log("[ANIM] parse fail"); return; }
    JSON_Object* robj = json_value_get_object(root);

    JSON_Object* framesObj = json_object_get_object(robj, "frames");
    JSON_Array* framesArr = json_object_get_array(robj, "frames");

    int atlasW = 0, atlasH = 0;
    if (s_bgAtlas) SDL_QueryTexture(s_bgAtlas, NULL, NULL, &atlasW, &atlasH);

    // frames as object
    if (framesObj) {
        size_t count = json_object_get_count(framesObj);
        SDL_Log("[ANIM] frames=object, count=%zu", count);
        for (size_t idx = 0; idx < count; ++idx) {
            const char* key = json_object_get_name(framesObj, idx);
            JSON_Object* fobj = json_object_get_object(framesObj, key);
            if (!fobj) continue;

            JSON_Object* rect = json_object_get_object(fobj, "frame");
            if (!rect) { SDL_Log("[ANIM] missing frame for key=%s", key); continue; }

            AnimFrame fr = { 0 };
            fr.rect.x = (int)json_object_get_number(rect, "x");
            fr.rect.y = (int)json_object_get_number(rect, "y");
            fr.rect.w = (int)json_object_get_number(rect, "w");
            fr.rect.h = (int)json_object_get_number(rect, "h");
            fr.duration_ms = (int)json_object_get_number(fobj, "duration");
            if (fr.duration_ms <= 0) fr.duration_ms = 100;

            if (fr.rect.w <= 0 || fr.rect.h <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] invalid WH key=%s", key);
                continue;
            }
            if (atlasW > 0 && atlasH > 0) {
                if (fr.rect.x < 0 || fr.rect.y < 0 ||
                    fr.rect.x + fr.rect.w > atlasW || fr.rect.y + fr.rect.h > atlasH) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "[ANIM] OOB key=%s rect=%d,%d,%d,%d atlas=%d,%d",
                        key, fr.rect.x, fr.rect.y, fr.rect.w, fr.rect.h, atlasW, atlasH);
                    continue;
                }
            }

            if (s_bgFrameCount < (int)SDL_arraysize(s_bgFrames))
                s_bgFrames[s_bgFrameCount++] = fr;
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] frame cap exceeded");
                break;
            }
        }
    }
    // frames as array
    else if (framesArr) {
        size_t count = json_array_get_count(framesArr);
        SDL_Log("[ANIM] frames=array, count=%zu", count);
        for (size_t idx = 0; idx < count; ++idx) {
            JSON_Object* fobj = json_array_get_object(framesArr, idx);
            if (!fobj) continue;

            JSON_Object* rect = json_object_get_object(fobj, "frame");
            if (!rect) { SDL_Log("[ANIM] missing frame at %zu", idx); continue; }

            AnimFrame fr = { 0 };
            fr.rect.x = (int)json_object_get_number(rect, "x");
            fr.rect.y = (int)json_object_get_number(rect, "y");
            fr.rect.w = (int)json_object_get_number(rect, "w");
            fr.rect.h = (int)json_object_get_number(rect, "h");
            fr.duration_ms = (int)json_object_get_number(fobj, "duration");
            if (fr.duration_ms <= 0) fr.duration_ms = 100;

            if (fr.rect.w <= 0 || fr.rect.h <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] invalid WH idx=%zu", idx);
                continue;
            }
            if (atlasW > 0 && atlasH > 0) {
                if (fr.rect.x < 0 || fr.rect.y < 0 ||
                    fr.rect.x + fr.rect.w > atlasW || fr.rect.y + fr.rect.h > atlasH) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "[ANIM] OOB idx=%zu rect=%d,%d,%d,%d atlas=%d,%d",
                        idx, fr.rect.x, fr.rect.y, fr.rect.w, fr.rect.h, atlasW, atlasH);
                    continue;
                }
            }

            if (s_bgFrameCount < (int)SDL_arraysize(s_bgFrames))
                s_bgFrames[s_bgFrameCount++] = fr;
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] frame cap exceeded");
                break;
            }
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[ANIM] no frames object/array");
    }

    json_value_free(root);

    if (s_bgFrameCount <= 0) {
        SDL_Log("[ANIM] parsed but no valid frames");
        return;
    }

    // 아틀라스 텍스처가 있다면 아틀라스 모드 사용(성능 좋음)
    if (s_bgAtlas) { s_bgFramesUseAtlas = true; return; }

    // (옵션) 아틀라스가 없다면 분할 텍스쳐 생성
    SDL_Surface* atlasSurface = IMG_Load(ASSETS_IMAGES_DIR "room.png");
    if (!atlasSurface) { SDL_Log("[ANIM] fallback surface load fail: %s", IMG_GetError()); s_bgFrameCount = 0; return; }

    if (atlasSurface->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(atlasSurface, SDL_PIXELFORMAT_RGBA32, 0);
        if (!converted) {
            SDL_Log("[ANIM] surface convert fail: %s", SDL_GetError());
            SDL_FreeSurface(atlasSurface);
            s_bgFrameCount = 0;
            return;
        }
        SDL_FreeSurface(atlasSurface);
        atlasSurface = converted;
    }

    for (int i = 0; i < s_bgFrameCount; ++i) {
        AnimFrame* fr = &s_bgFrames[i];
        SDL_Surface* frameSurface =
            SDL_CreateRGBSurfaceWithFormat(0, fr->rect.w, fr->rect.h, 32, atlasSurface->format->format);
        if (!frameSurface) { SDL_Log("[ANIM] create frame surface fail: %s", SDL_GetError()); continue; }

        if (SDL_BlitSurface(atlasSurface, &fr->rect, frameSurface, NULL) < 0) {
            SDL_Log("[ANIM] blit fail: %s", SDL_GetError());
            SDL_FreeSurface(frameSurface);
            continue;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(G_Renderer, frameSurface);
        SDL_FreeSurface(frameSurface);
        if (!tex) { SDL_Log("[ANIM] create texture fail: %s", SDL_GetError()); continue; }
        fr->texture = tex;
    }

    SDL_FreeSurface(atlasSurface);

    // 유효 텍스쳐만 남기기(compact)
    int write = 0;
    for (int read = 0; read < s_bgFrameCount; ++read) {
        if (!s_bgFrames[read].texture) continue;
        if (write != read) s_bgFrames[write] = s_bgFrames[read];
        ++write;
    }
    s_bgFrameCount = write;

    if (s_bgFrameCount == 0) {
        SDL_Log("[ANIM] no usable splitted textures");
        return;
    }
    SDL_Log("[ANIM] using fallback splitted textures");
}

// -----------------------------
// Scene 콜백
// -----------------------------
static void init(void* arg)
{
    srand(time(NULL));
    int idx = -1;
    if (arg) idx = (int)(intptr_t)arg;
    else if (G_SelectedPlantIndex >= 0) idx = G_SelectedPlantIndex;

    if (idx < 0) { scene_switch(SCENE_SELECT_PLANT); return; }

    G_SelectedPlantIndex = idx;
    s_plant = plantdb_get(idx);
    if (!s_plant) { scene_switch(SCENE_SELECT_PLANT); return; }

    if (s_bgFrameCount <= 0) {
        load_background_animation();
    }

    SDL_Log("[GAME] bg frames loaded: count=%d, useAtlas=%d", s_bgFrameCount, (int)s_bgFramesUseAtlas);

    s_waterCount = 0;
    s_windowOpen = false;
    s_light_level = 0;

    SDL_Log("[GAME] bg frames loaded: count=%d, useAtlas=%d", s_bgFrameCount, (int)s_bgFramesUseAtlas);

    s_waterCount = 0;
    s_windowOpen = false;

    if (!s_pot) {
        s_pot = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "pot.png");
        if (!s_pot) SDL_Log("GAMEPLAY: load pot.png fail: %s", IMG_GetError());
    }

    if (!s_bgDay) {

        s_bgDay = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "sunny.png");
        if (!s_bgDay) SDL_Log("GAMEPLAY: load bg_day.png fail: %s", IMG_GetError());
    }

    if (!s_bgNight) {

        s_bgNight = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "night.png");
        if (!s_bgNight) SDL_Log("GAMEPLAY: load bg_night.png fail: %s", IMG_GetError());
    }

    if (!s_bgSunrise) {

        s_bgSunrise = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "sunset.png");
        if (!s_bgSunrise) SDL_Log("GAMEPLAY: load bg_sunrise.png fail: %s", IMG_GetError());
    }


    if (!s_weatherClouds) {
        s_weatherClouds = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "cloud.png");
    }

    if (!s_weatherRain) {

        s_weatherRain = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "rainy.png"); // ?
    }

    if (!s_weatherSnow) {

        s_weatherSnow = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "snowy.png");
    }



    if (!s_bgTexture) {
        s_bgTexture = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "select_Background.png");
        if (!s_bgTexture) SDL_Log("GAMEPLAY: load select_Background.png fail: %s", IMG_GetError());
    }



    if (!s_backIcon) {
        s_backIcon = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back.png");
        if (!s_backIcon) SDL_Log("GAMEPLAY: load I_exit.png fail: %s", IMG_GetError());
    }
    if (!s_water) {
        s_water = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_water.png");
        if (!s_water) SDL_Log("GAMEPLAY: load I_water.png fail: %s", IMG_GetError());
    }
    if (!s_window) {
        s_window = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_window.png");
        if (!s_window) SDL_Log("GAMEPLAY: load I_window.png fail: %s", IMG_GetError());
    }

    if (!s_nobug) {
        s_nobug = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_nobug.png");
        if (!s_nobug) SDL_Log("GAMEPLAY: load I_nobug.png fail: %s", IMG_GetError());
        else SDL_Log("loading success");
    }
    if (!s_nogom) {
        s_nogom = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_nogom.png");
        if (!s_nogom) SDL_Log("GAMEPLAY: load I_nogom.png fail: %s", IMG_GetError());
        else SDL_Log("loading success");
    }
    if (!s_ifhot) {
        s_ifhot = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_ifhot.png");
        if (!s_ifhot) SDL_Log("GAMEPLAY: load I_ifhot.png fail: %s", IMG_GetError());
        else SDL_Log("loading success");
    }
    if (!s_ifcold) {
        s_ifcold = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_ifcold.png");
        if (!s_ifcold) SDL_Log("GAMEPLAY: load I_ifcold.png fail: %s", IMG_GetError());
        else SDL_Log("loading success");
    }
    if (!s_lamp) {
        s_lamp = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_lampon.png");
        if (!s_lamp) SDL_Log("GAMEPLAY: load I_nobug.png fail: %s", IMG_GetError());
        else SDL_Log("loading success");
    }
    if (!s_biryo) {
        s_biryo = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_biryo.png");
        if (!s_biryo) SDL_Log("GAMEPLAY: load I_nobug.png fail: %s", IMG_GetError());
        else SDL_Log("loading success");
    }
    if (!s_lamp_hotbar) {
        s_lamp_hotbar = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "lamp_hotbar.png");
        if (!s_biryo) SDL_Log("GAMEPLAY: load lamp_hotbar.png fail: %s", IMG_GetError());
        else SDL_Log("hotbar loading success");
    }
    if (!s_lamp_levelup) {
        s_lamp_levelup = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "upper.png");
        if (!s_lamp_levelup) SDL_Log("GAMEPLAY: load upper.png fail: %s", IMG_GetError());
        else SDL_Log("upper loading success");
    }
    if (!s_lamp_leveldown) {
        s_lamp_leveldown = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "lower.png");
        if (!s_lamp_leveldown) SDL_Log("GAMEPLAY: load lower.png fail: %s", IMG_GetError());
        else SDL_Log("lower loading success");
    }
    if (!s_lamp_leveldown) {
        s_lamp_leveldown = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "lower.png");
        if (!s_lamp_leveldown) SDL_Log("GAMEPLAY: load lower.png fail: %s", IMG_GetError());
        else SDL_Log("lower loading success");
    }

    if (!s_exit) {
        s_exit = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_exit.png");
        if (!s_lamp_leveldown) SDL_Log("GAMEPLAY: load I_exit.png fail: %s", IMG_GetError());
        else SDL_Log("exit loading success");
    }

    if (!s_texBugIdle)
    {
        s_texBugIdle = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "event_bugs.png");
        if (!s_texBugIdle)
            SDL_Log("GAMEPLAY: load event_bugs.png fail: %s", IMG_GetError());
    }
    if (!s_texMoldIdle)
    {
        s_texMoldIdle = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "event_gompang.png");
        if (!s_texMoldIdle)
            SDL_Log("GAMEPLAY: load event_gompang.png fail: %s", IMG_GetError());
    }
    if (!s_texBugSpray)
    {
        s_texBugSpray = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "event_bugSpray.png");
        if (!s_texBugSpray)
            SDL_Log("GAMEPLAY: load event_bugSpray.png fail: %s", IMG_GetError());
    }
    if (!s_texMoldSpray)
    {
        s_texMoldSpray = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "event_gompangSpray.png");
        if (!s_texMoldSpray)
            SDL_Log("GAMEPLAY: load event_gompangSpray.png fail: %s", IMG_GetError());
    }

    s_bugIdleAnim.totalFrames = BUG_IDLE_FRAMES;
    s_bugIdleAnim.frameDuration = 1.0f / IDLE_FPS;
    s_moldIdleAnim.totalFrames = MOLD_IDLE_FRAMES;
    s_moldIdleAnim.frameDuration = 1.0f / IDLE_FPS;

    s_bugSprayAnim.totalFrames = BUG_SPRAY_FRAMES;
    s_bugSprayAnim.frameDuration = 1.0f / SPRAY_FPS;
    s_moldSprayAnim.totalFrames = MOLD_SPRAY_FRAMES;
    s_moldSprayAnim.frameDuration = 1.0f / SPRAY_FPS;



    // 날씨 데이터 초기 로드
    if (weather_load(&g_weather)) {
        weather_compute_sun_secs(&g_weather);
    }
    g_last_weather_check = SDL_GetTicks();
    g_timeOfDay = weather_get_time_of_day(&g_weather);
    g_last_tod_check = SDL_GetTicks();

    // 오디오
    if (Mix_PlayingMusic()) {
        Mix_FadeOutMusic(200);
    }
    gameplay_ensure_bgm_loaded();
    Mix_HookMusicFinished(gameplay_on_music_finished);

    int bgm_idx = pick_random_index();
    s_lastBgm = bgm_idx;
    if (s_gameBGMs[bgm_idx]) {
        if (Mix_FadeInMusic(s_gameBGMs[bgm_idx], /*loops=*/1, /*ms=*/200) < 0) {
            SDL_Log("[GAMEPLAY BGM] start error: %s", Mix_GetError());
        }
    }

    SDL_Texture* texbackHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back_hover.png");
    SDL_Texture* texbackPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_back_pressed.png");
    ui_button_set_icons(&s_btnBack, s_backIcon, texbackHover, texbackPressed);

    SDL_Texture* texbiryoHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_biryo_hover.png");
    SDL_Texture* texbiryoPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_biryo_pressed.png");
    ui_button_set_icons(&s_btnbiryo, s_biryo, texbiryoHover, texbiryoPressed);

    SDL_Texture* texcoldHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_ifCold_hover.png");
    SDL_Texture* texcoldPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_ifCold_pressed.png");
    ui_button_set_icons(&s_btnifcold, s_ifcold, texcoldHover, texcoldPressed);
    //////////////////////////////////////////////////////////////////////////////
    SDL_Texture* texhotHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_ifhot_hover.png");
    SDL_Texture* texhotPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_ifhot_pressed.png");
    ui_button_set_icons(&s_btnifhot, s_ifhot, texhotHover, texhotPressed);

    SDL_Texture* texlampHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_lampOn_hover.png");
    SDL_Texture* texlampPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_lampOn_pressed.png");
    ui_button_set_icons(&s_btnlamp, s_lamp, texlampHover, texlampPressed);

    SDL_Texture* texnobugHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_nobug_hover.png");
    SDL_Texture* texnobugPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_nobug_pressed.png");
    ui_button_set_icons(&s_btnnobug, s_nobug, texnobugHover, texnobugPressed);

    SDL_Texture* texnogomHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_nogom_hover.png");
    SDL_Texture* texnogomPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_nogom_pressed.png");
    ui_button_set_icons(&s_btnnogom, s_nogom, texnogomHover, texnogomPressed);

    SDL_Texture* texwaterHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_water_hover.png");
    SDL_Texture* texwaterPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_water_pressed.png");
    ui_button_set_icons(&s_btnWater, s_water, texwaterHover, texwaterPressed);

    SDL_Texture* texwindowHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_window_hover.png");
    SDL_Texture* texwindowPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_window_pressed.png");
    ui_button_set_icons(&s_btnWindow, s_window, texwindowHover, texwindowPressed);
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    SDL_Texture* texlpdownHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "lower_hover.png");
    SDL_Texture* texlpdownPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "lower_pressed.png");
    ui_button_set_icons(&s_btnlpdown, s_lamp_leveldown, texlpdownHover, texlpdownPressed);

    SDL_Texture* texlpupHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "upper_hover.png");
    SDL_Texture* texlpupPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "upper_pressed.png");
    ui_button_set_icons(&s_btnlpup, s_lamp_levelup, texlpupHover, texlpupPressed);

    SDL_Texture* texexitHover = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_exit.png");
    SDL_Texture* texexitPressed = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "I_exit.png");
    ui_button_set_icons(&s_btnexit, s_exit, texexitHover, texexitPressed);


    s_status.moisture = 60.f;
    s_status.temp = (float)s_room_temperature; // 방 온도 따라감
    s_status.humidity = 60.f;
    s_status.light = 0.f;
    s_status.happiness = 70.f;
    s_status.nutrition = 70.f;

    if (s_plantLevel == 1) {
        s_monstera = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv1.png");
    }
    else if (s_plantLevel == 2) {
        s_monstera = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv2.png");
        //anim_load_from_json(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv2.png", ASSETS_DIR "data/monsteraLv2.json", s_plantFrames, 64, &s_monstera, &s_plantFrameCount);

    }
    else if (s_plantLevel == 3) {
        s_monstera = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv3.png");
        //anim_load_from_json(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv3.png", ASSETS_DIR "data/monsteraLv3.json", s_plantFrames, 64, &s_monstera, &s_plantFrameCount);
    }

    

    settings_apply_audio();
    layout();
}

static void handle(SDL_Event* e)
{
    if (e->type == SDL_QUIT) { G_Running = 0; return; }

    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        layout();
    }

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) { scene_switch_fade(SCENE_MAINMENU, 0.2f, 0.4f); return; }
        if (e->key.keysym.sym == SDLK_w) { on_window(NULL); }
        if (e->key.keysym.sym == SDLK_SPACE) { on_water(NULL); }
    }

    ui_button_handle(&s_btnBack, e);
    ui_button_handle(&s_btnWater, e);
    ui_button_handle(&s_btnWindow, e);
    ui_button_handle(&s_btnlamp, e);
    ui_button_handle(&s_btnnogom, e);
    ui_button_handle(&s_btnnobug, e);
    ui_button_handle(&s_btnifhot, e);
    ui_button_handle(&s_btnifcold, e);
    ui_button_handle(&s_btnbiryo, e);

    if (lamp_panel == 1) {
        ui_button_handle(&s_btnlpup, e);
        ui_button_handle(&s_btnlpdown, e);
    }

    if (back_panel) {
        ui_button_handle(&s_btnexit, e);
    }
}

static void update(float dt)
{
    if (s_bgFrameCount > 0) {
        s_bgFrameElapsedMs += dt * 1000.f;

        int duration = s_bgFrames[s_bgFrameIndex].duration_ms;
        if (duration <= 0) duration = 100;

        while (s_bgFrameElapsedMs >= duration && s_bgFrameCount > 0) {
            s_bgFrameElapsedMs -= duration;
            s_bgFrameIndex = (s_bgFrameIndex + 1) % s_bgFrameCount;

            duration = s_bgFrames[s_bgFrameIndex].duration_ms;
            if (duration <= 0) duration = 100;
        }
    }

    update_weather_if_needed();

    // 시간대는 10초마다 한 번만 다시 계산
    Uint32 now = SDL_GetTicks();
    if (now - g_last_tod_check >= TOD_INTERVAL_MS) {
        g_last_tod_check = now;
        g_timeOfDay = weather_get_time_of_day(&g_weather);
        // printf("[TIMEOFDAY] mode=%d\n", g_timeOfDay);
    }


    if (s_plantLevel == 1) {
        s_monstera = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv1.png");
    }
    else if (s_plantLevel == 2) {
        s_monstera = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv2.png");
        //anim_load_from_json(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv2.png", ASSETS_DIR "data/monsteraLv2.json", s_plantFrames, 64, &s_monstera, &s_plantFrameCount);

    }
    else if (s_plantLevel == 3) {
        s_monstera = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv3.png");
        //anim_load_from_json(G_Renderer, ASSETS_IMAGES_DIR "monsteraLv3.png", ASSETS_DIR "data/monsteraLv3.json", s_plantFrames, 64, &s_monstera, &s_plantFrameCount);
    }

    ////////////////////////////////////////////////////////////////////////////
    int w = 1920, h = 1080;

    // ---------------- 시간대 계산 ----------------
    TimeOfDay tod = weather_get_time_of_day(&g_weather);

    bool hasAtlasAnim = (s_bgFramesUseAtlas && s_bgAtlas && s_bgFrameCount > 0);

    if (tod == TIMEOFDAY_SUNRISE || tod == TIMEOFDAY_SUNSET) {
        // ✅ 일출/일몰 시간대: sunset 애니메이션 사용

        if (g_weather.tag == WEATHER_TAG_CLEAR) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "sunset.png");

            background_change = false;
        }
        else if (g_weather.tag == WEATHER_TAG_CLOUDY) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "cloud.png");

            background_change = false;
        }
        else if (g_weather.tag == WEATHER_TAG_RAIN) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "rainy.png");

            background_change = false;
        }
        else {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "snowy.png");

            background_change = false;
        }


    }
    else if (tod == TIMEOFDAY_DAY) {
        // ✅ 낮: 낮 배경 이미지
        if (g_weather.tag == WEATHER_TAG_CLEAR) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "sunny.png");

            background_change = false;
        }
        else if (g_weather.tag == WEATHER_TAG_CLOUDY) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "cloud.png");

            background_change = false;
        }
        else if (g_weather.tag == WEATHER_TAG_RAIN) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "rainy.png");

            background_change = false;
        }
        else {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "snowy.png");

            background_change = false;
        }

    }
    else { // ✅ TIMEOFDAY_NIGHT
        if (g_weather.tag == WEATHER_TAG_CLEAR) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "night.png");

            background_change = false;
        }
        else if (g_weather.tag == WEATHER_TAG_CLOUDY) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "cloud.png");

            background_change = false;
        }
        else if (g_weather.tag == WEATHER_TAG_RAIN) {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "rainy.png");

            background_change = false;
        }
        else {
            background_change = true;

            SDL_DestroyTexture(s_bgAtlas);
            s_bgAtlas = NULL;
            s_bgAtlas = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "snowy.png");

            background_change = false;
        }

    }



    if (now - g_last_tod_check >= TOD_INTERVAL_MS) {
        g_last_tod_check = now;

        TimeOfDay newTod = weather_get_time_of_day(&g_weather);

        // 시간대가 바뀌었을 때만 페이드 시작
        if (newTod != g_timeOfDay) {
            g_tod_before = g_timeOfDay;
            g_tod_after = newTod;
            g_tod_fade_active = 1;
            g_tod_fade_t = 0.0f;

            // 실제 적용은 페이드 중간/끝에서 할 거라 여기선 g_timeOfDay 유지 or 바로 바꿔도 됨
            // 나는 "현재 상태 기준으로 렌더 + 페이드 오버레이" 방식쓸 거라 여기서 바꾸지 않아도 됨.
        }
    }

    // ---- 페이드 타이머 업데이트 ----
    if (g_tod_fade_active) {
        g_tod_fade_t += dt;
        if (g_tod_fade_t >= TOD_FADE_TIME) {
            g_tod_fade_active = 0;
            g_tod_fade_t = TOD_FADE_TIME;
            // 페이드 끝나면 실제 시간대 값을 전환 후 상태로 고정
            g_timeOfDay = g_tod_after;
        }
    }

   
    if (now - g_last_tod_check >= TOD_INTERVAL_MS) {
        g_last_tod_check = now;
        TimeOfDay newTod = weather_get_time_of_day(&g_weather);

        if (newTod != g_timeOfDay) {
            g_tod_before = g_timeOfDay;
            g_tod_after = newTod;
            g_tod_fade_active = 1;
            g_tod_fade_t = 0.0f;
        }
    }

    if (g_tod_fade_active) {
        g_tod_fade_t += dt;
        if (g_tod_fade_t >= TOD_FADE_TIME) {
            g_tod_fade_active = 0;
            g_tod_fade_t = TOD_FADE_TIME;
            g_timeOfDay = g_tod_after;
        }
    }


    // 랜덤 이벤트들
    cooltime -= dt;

    if (cooltime <= 0.f) {
        update_weather_event(dt);   // 랜덤 날씨 이벤트
    }

    update_random_events(dt);   // 벌레, 곰팡이 생성

    // 상태값 업데이트
    update_moisture(dt);
    update_temperature(dt);
    update_humidity(dt);
    update_happiness(dt);
    update_nutrition(dt);

    update_spray_anims(dt);
    // 이벤트 애니메이션 진행
    event_anim_update(dt);
}

static void render(SDL_Renderer* r)
{
    int w, h; SDL_GetRendererOutputSize(r, &w, &h);

    TimeOfDay tod = weather_get_time_of_day(&g_weather);

    if (background_change) {
        float half = TOD_FADE_TIME * 0.5f;

        if (g_tod_fade_t < half) {
            tod = g_tod_before;
        }
        else {
            tod = g_tod_after;
        }
    }

    bool hasAtlasAnim = (s_bgFramesUseAtlas && s_bgAtlas && s_bgFrameCount > 0);

    if (hasAtlasAnim) {
        SDL_Rect dst = { 0,0,w,h };
        SDL_Rect src = s_bgFrames[s_bgFrameIndex].rect;
        SDL_RenderCopy(r, s_bgAtlas, &src, &dst);
    }

  

    render_time_tint(r, tod, w, h);

    if (background_change) {
        float t = g_tod_fade_t / TOD_FADE_TIME;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        float alphaNorm;
        if (t < 0.5f) alphaNorm = t / 0.5f;
        else          alphaNorm = (1.0f - t) / 0.5f;

        Uint8 a = (Uint8)(alphaNorm * 255);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, a);
        SDL_Rect full = { 0, 0, w, h };
        SDL_RenderFillRect(r, &full);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    if (!window_open) {
        if (s_room) {
            SDL_Rect dst = { 0,0,w,h };
            SDL_RenderCopy(r, s_room, NULL, &dst);
        }
    }
    else {
        if (s_room_open) {
            SDL_Rect dst = { 0,0,w,h };
            SDL_RenderCopy(r, s_room_open, NULL, &dst);
        }
    }

    SDL_Color title = { 240,236,228,255 };
    SDL_Color body = { 240,236,228,255 };
    SDL_Color clock = { 240,236,228,255 };
    int x = 140, y = 140;
    draw_text(r, title, x, y, "%s", s_plant->name_kr);
    draw_text(r, body, x, y + 36, "물 준 횟수: %d회 · 권장 %d일", s_waterCount, s_plant->water_days);
    draw_text(r, body, x, y + 72, "창문: %s", s_windowOpen ? "열림" : "닫힘");
    draw_text(r, body, x, y + 108, "빛 세기: %d", s_light_level);
    draw_text(r, body, x, y + 144, "방 온도: %d", s_room_temperature);

    char timebuf[32];
    char daybuf[32];
    get_current_time_string(timebuf, sizeof(timebuf));
    get_current_day_string(daybuf, sizeof(daybuf));
    draw_text2(r, clock, x + 1500, y, 140, 37, "%s", timebuf);
    draw_text2(r, clock, x + 1500, y + 40, 160, 37, "%s", daybuf);

    ui_button_render(r, G_FontMain, &s_btnBack, NULL);
    ui_button_render(r, G_FontMain, &s_btnWater, NULL);
    ui_button_render(r, G_FontMain, &s_btnWindow, NULL);
    ui_button_render(r, G_FontMain, &s_btnnobug, NULL);
    ui_button_render(r, G_FontMain, &s_btnnogom, NULL);
    ui_button_render(r, G_FontMain, &s_btnifhot, NULL);
    ui_button_render(r, G_FontMain, &s_btnifcold, NULL);
    ui_button_render(r, G_FontMain, &s_btnlamp, NULL);
    ui_button_render(r, G_FontMain, &s_btnbiryo, NULL);

    SDL_Rect pot_location = (SDL_Rect){ 0,0, w, h };
    SDL_RenderCopy(r, s_pot, NULL, &pot_location);

    // ★ 이벤트 애니메이션 렌더

    if (s_plantLevel == 1) {
        SDL_Rect plant_location = (SDL_Rect){ w / 2 - 10, h - 330, 6 * 3, 17 * 3 };
        SDL_RenderCopy(r, s_monstera, NULL, &plant_location);
    }
    else if (s_plantLevel == 2) {
        SDL_Rect plant_location = (SDL_Rect){ w / 2 - 70, h - 390, 56 * 2, 57 * 2 };
        SDL_RenderCopy(r, s_monstera, NULL, &plant_location);
    }
    else if (s_plantLevel == 3) {
        SDL_Rect plant_location = (SDL_Rect){ w / 2 - 125, h - 545, 116 * 2, 132 * 2 };
        SDL_RenderCopy(r, s_monstera, NULL, &plant_location);
    }
    
    /*
    if (s_plantLevel == 1) {
        SDL_RenderCopy(r, s_monstera, NULL, &plant_location);
    }
    else {
        render_plant_anim(r, 0, 0);
    }
    */


    render_bug_mold(r, w, h);

    // 스프레이 애니 렌더 (버그/곰팡이 제거 시)🐥
    render_spray_anims(r, w, h);

    event_anim_render(r);

    if (s_hasBug == true) {
        SDL_Color bug = {255,0,0,255};
        draw_text(r, bug, w / 2, 300, " 벌레가 나타났습니다!");
    }
    if (s_hasMold == true) {
        SDL_Color bug = { 0,255,0,255 };
        draw_text(r, bug, w / 2, 350, " 곰팡이가 나타났습니다!");
    }

    if (lamp_panel == 1) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 88, 88, 88, 180);
        SDL_Rect panel = { 560, 870, 160, 100 };
        SDL_RenderFillRect(r, &panel);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        ui_button_render(r, G_FontMain, &s_btnlpup, NULL);
        ui_button_render(r, G_FontMain, &s_btnlpdown, NULL);
    }

    if (back_panel) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 90, 53, 32, 180);
        SDL_Rect panel = { w / 2 - 480, h / 2 - 270, 160 * 6, 90 * 6 };
        SDL_RenderFillRect(r, &panel);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        draw_text(r, body, w / 2 - 140, h / 2, "메인 메뉴로 나가시겠습니까?");
        draw_text(r, body, w / 2 - 35, h / 2 + 150, "나가기");
        ui_button_render(r, G_FontMain, &s_btnexit, NULL);
    }
}

static void cleanup(void)
{
    destroy_frame_textures();
    if (s_bgAtlas) { SDL_DestroyTexture(s_bgAtlas);       s_bgAtlas = NULL; }
    if (s_bgTexture) { SDL_DestroyTexture(s_bgTexture);     s_bgTexture = NULL; }
    if (s_backIcon) { SDL_DestroyTexture(s_backIcon);      s_backIcon = NULL; }
    if (s_water) { SDL_DestroyTexture(s_water);         s_water = NULL; }
    if (s_window) { SDL_DestroyTexture(s_window);        s_window = NULL; }
    if (s_nobug) { SDL_DestroyTexture(s_nobug);         s_nobug = NULL; }
    if (s_nogom) { SDL_DestroyTexture(s_nogom);         s_nogom = NULL; }
    if (s_ifhot) { SDL_DestroyTexture(s_ifhot);         s_ifhot = NULL; }
    if (s_ifcold) { SDL_DestroyTexture(s_ifcold);        s_ifcold = NULL; }
    if (s_lamp) { SDL_DestroyTexture(s_lamp);          s_lamp = NULL; }
    if (s_biryo) { SDL_DestroyTexture(s_biryo);         s_biryo = NULL; }
    if (s_lamp_hotbar) { SDL_DestroyTexture(s_lamp_hotbar);   s_lamp_hotbar = NULL; }
    if (s_lamp_levelup) { SDL_DestroyTexture(s_lamp_levelup);  s_lamp_levelup = NULL; }
    if (s_lamp_leveldown) { SDL_DestroyTexture(s_lamp_leveldown);s_lamp_leveldown = NULL; }

    if (s_weatherClouds) { SDL_DestroyTexture(s_weatherClouds); s_weatherClouds = NULL; }
    if (s_weatherRain) { SDL_DestroyTexture(s_weatherRain);   s_weatherRain = NULL; }
    if (s_weatherSnow) { SDL_DestroyTexture(s_weatherSnow);   s_weatherSnow = NULL; }

    if (s_texBugIdle)
    {
        SDL_DestroyTexture(s_texBugIdle);
        s_texBugIdle = NULL;
    }
    if (s_texMoldIdle)
    {
        SDL_DestroyTexture(s_texMoldIdle);
        s_texMoldIdle = NULL;
    }
    if (s_texBugSpray)
    {
        SDL_DestroyTexture(s_texBugSpray);
        s_texBugSpray = NULL;
    }
    if (s_texMoldSpray)
    {
        SDL_DestroyTexture(s_texMoldSpray);
        s_texMoldSpray = NULL;
    }


    destroy_event_frames();

    Mix_HookMusicFinished(NULL);

    for (int i = 0; i < (int)SDL_arraysize(kGameBGMs); ++i) {
        if (s_gameBGMs[i]) { Mix_FreeMusic(s_gameBGMs[i]); s_gameBGMs[i] = NULL; }
    }
    s_bgmLoaded = 0;
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Gameplay" };
Scene* scene_gameplay_object(void) { return &SCENE_OBJ; }
