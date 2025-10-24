#include "../include/loading.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/ui.h"
#include <SDL2/SDL_image.h> // 로딩 아이콘 이미지 쓰고 싶으면

static struct {
    SceneID target;
    LoadingJobFn job;
    void* userdata;
    SDL_Thread* thread;
    int progress;
    int done;
    float wait_after_done; // 끝난 뒤 잠깐 보여줄 시간(연출용)
    float timer;
} LD;

static SDL_Texture* s_spinner = NULL;  // assets/images/spinner.png (선택)
static float spin_deg = 0.f;

static UIProgressBar s_pb;



// -------------------- 내부: 스레드에서 하는 일 --------------------
static int loading_thread(void* p) {
    // 실제 작업이 있으면 실행하면서 진행률을 갱신 (예: 0→100)
    if (LD.job) {
        LD.progress = 0;
        LD.job(LD.userdata); // 내부에서 진행률을 조절하고 싶다면 전역 LD.progress 포인터를 넘기는 방식으로 바꿔도 됨
        LD.progress = 100;
    }
    else {
        // 간단한 페이크 로딩 (연출용)
        for (int i = 0;i <= 100;i += 5) { LD.progress = i; SDL_Delay(20); }
    }
    LD.done = 1;
    return 0;
}

// -------------------- Scene 콜백들 --------------------
static void init(void *arg) {
    (void)arg;
    LD.thread = NULL;
    LD.done = 0;
    LD.timer = 0.f;
    LD.wait_after_done = 0.25f; // 로딩이 끝나도 0.25초 정도는 보여주기

    if (!s_spinner) {
        s_spinner = IMG_LoadTexture(G_Renderer, ASSETS_IMAGES_DIR "spinner.png"); // 없으면 NULL이어도 OK
    }

    // 작업 스레드 시작
    LD.thread = SDL_CreateThread(loading_thread, "loading_thread", NULL);

    int w, h; SDL_GetRendererOutputSize(G_Renderer, &w, &h);
    // 화면 중앙 하단 쪽에 가로 420, 높이 22
    ui_progress_init(&s_pb, (SDL_Rect) { (w - 420) / 2, (h / 2) + 60, 420, 22 });
    // 작업 스레드 시작
    LD.thread = SDL_CreateThread(loading_thread, "loading_thread", NULL);
}

static void handle(SDL_Event* e) {
    if (e->type == SDL_QUIT) { G_Running = 0; }
}

static void update(float dt) {
    spin_deg += 360.f * dt * 0.8f;

    if (LD.done) {
        LD.timer += dt;
        if (LD.timer >= LD.wait_after_done) {
            // 완료 → 목표 씬으로 페이드 전환
            scene_switch_fade(LD.target, 0.0f, 0.35f);
            // (주의) 여기서 스레드 Join까지 하고 싶으면 아래처럼:
            if (LD.thread) { SDL_WaitThread(LD.thread, NULL); LD.thread = NULL; }
        }
    }
    ui_progress_set(&s_pb, LD.progress / 100.0f);
}

static void render(SDL_Renderer* r) {
    // 배경
    SDL_SetRenderDrawColor(r, 14, 18, 26, 255);
    

    int w, h; SDL_GetRendererOutputSize(r, &w, &h);

    // “Loading…” 텍스트
    if (G_FontMain) {
        char buf[64];
        SDL_Color fg = { 220, 230, 235, 255 };
        SDL_snprintf(buf, sizeof(buf), "Loading... %d%%", (int)LD.progress);

        SDL_Surface* s = TTF_RenderUTF8_Blended(G_FontMain, buf, fg);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { (w - tw) / 2, (h - th) / 2 - 20, tw, th };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    // 스피너(선택)
    if (s_spinner) {
        int sw = 64, sh = 64;
        SDL_Rect dst = { w / 2 - sw / 2, h / 2 + 20, sw, sh };
        SDL_RenderCopyEx(r, s_spinner, NULL, &dst, spin_deg, NULL, SDL_FLIP_NONE);
    }

    // 하단 힌트(선택)
    if (G_FontMain) {
        SDL_Color c = { 180, 190, 200, 200 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(G_FontMain, "Tip: ESC to cancel (if supported)", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
            SDL_Rect d = { w - tw - 20, h - th - 16, tw, th };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }

    ui_progress_render(r, G_FontMain, &s_pb);
}

static void cleanup(void) {
    if (LD.thread) { SDL_WaitThread(LD.thread, NULL); LD.thread = NULL; }
    // s_spinner는 프로젝트 전역에서 쓸 수 있으니 굳이 여기서 파괴 안 해도 됨
    // 필요 시: SDL_DestroyTexture(s_spinner); s_spinner=NULL;
}

static Scene SCENE_OBJ = { init, handle, update, render, cleanup, "Loading" };
Scene* scene_loading_object(void) { return &SCENE_OBJ; }

// -------------------- 공개 API --------------------
void loading_begin(SceneID target, LoadingJobFn job, void* userdata,
    float fade_out_sec, float fade_in_sec)
{
    LD.target = target;
    LD.job = job;
    LD.userdata = userdata;
    LD.progress = 0;
    LD.done = 0;

    // 현재 씬 → 로딩 씬으로 페이드 아웃/인
    scene_switch_fade(SCENE_LOADING, fade_out_sec, 0.2f);
}
