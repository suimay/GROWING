#include "scene_manager.h"
#include "game.h"

Scene* G_Scenes[SCENE__COUNT] = { 0 };
SceneID G_CurrentScene = SCENE_MAINMENU;

void scene_register(SceneID id, Scene* sc) {
    if (id < 0 || id >= SCENE__COUNT) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[SCENE] register out of range id=%d", id);
        return;
    }
    G_Scenes[id] = sc;
    //SDL_Log("[SCENE] registered id=%d (%s)", id, sc ? sc->debug_name : "(null)");
}

static void do_switch(SceneID id) {
    if (id < 0 || id >= SCENE__COUNT) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[SCENE] invalid id=%d", id);
        return;
    }
    if (G_Scenes[G_CurrentScene] && G_Scenes[G_CurrentScene]->cleanup)
        G_Scenes[G_CurrentScene]->cleanup();

    G_CurrentScene = id;

    if (!G_Scenes[id]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[SCENE] id=%d not registered", id);
        return;
    }
    if (G_Scenes[id]->init)
        G_Scenes[id]->init();
}

void scene_switch(SceneID id) {
    do_switch(id);
}

typedef enum { TR_NONE = 0, TR_OUT, TR_SWITCH, TR_IN } TrPhase;

static struct {
    int active;
    TrPhase phase;
    SceneID target;
    float t;          // 현재 진행 시간
    float out_dur;    // 페이드 아웃 시간
    float in_dur;     // 페이드 인 시간
} g_tr = { 0 };

void scene_switch_fade(SceneID target, float out_sec, float in_sec) {
    // 이미 진행 중이면 덮어쓰기(원하면 무시하도록 바꿔도 됨)
    g_tr.active = 1;
    g_tr.phase = TR_OUT;
    g_tr.target = target;
    g_tr.t = 0.f;
    g_tr.out_dur = (out_sec <= 0.f) ? 0.001f : out_sec;
    g_tr.in_dur = (in_sec <= 0.f) ? 0.001f : in_sec;
}

int scene_is_transitioning(void) { return g_tr.active != 0; }

void scene_handle(SDL_Event* e) {
    if (G_Scenes[G_CurrentScene] && G_Scenes[G_CurrentScene]->handle)
        G_Scenes[G_CurrentScene]->handle(e);
}

void scene_update(float dt) {
    if (G_Scenes[G_CurrentScene] && G_Scenes[G_CurrentScene]->update)
        G_Scenes[G_CurrentScene]->update(dt);

    // 트랜지션 업데이트
    if (!g_tr.active) return;

    g_tr.t += dt;
    switch (g_tr.phase) {
    case TR_OUT:
        if (g_tr.t >= g_tr.out_dur) {
            // 스위치 타이밍
            do_switch(g_tr.target);
            g_tr.phase = TR_IN;
            g_tr.t = 0.f;
        }
        break;
    case TR_IN:
        if (g_tr.t >= g_tr.in_dur) {
            g_tr.active = 0;
            g_tr.phase = TR_NONE;
            g_tr.t = 0.f;
        }
        break;
    default: break;
    }
}

void scene_render(SDL_Renderer* r) {
    if (G_Scenes[G_CurrentScene] && G_Scenes[G_CurrentScene]->render)
        G_Scenes[G_CurrentScene]->render(r);

    // 트랜지션 오버레이
    if (!g_tr.active) return;

    float alpha = 0.f;
    if (g_tr.phase == TR_OUT) {
        float k = g_tr.t / g_tr.out_dur;
        if (k > 1.f) k = 1.f;
        alpha = k;                 // 0→1
    }
    else if (g_tr.phase == TR_IN) {
        float k = g_tr.t / g_tr.in_dur;
        if (k > 1.f) k = 1.f;
        alpha = 1.f - k;           // 1→0
    }

    // 검은 오버레이
    int w, h; SDL_GetRendererOutputSize(r, &w, &h);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(alpha * 255));
    SDL_Rect full = { 0,0,w,h };
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void scene_cleanup(void) {
    if (G_Scenes[G_CurrentScene] && G_Scenes[G_CurrentScene]->cleanup)
        G_Scenes[G_CurrentScene]->cleanup();
}
