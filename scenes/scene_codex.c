#include "../include/scene.h"
#include "../scene_manager.h"
#include "../game.h"
#include "../include/core.h"
#include "../include/save.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>


// --외부 훅
extern int plantdb_count(void);
extern const PlantInfo* plantdb_get(int idx);
// 추후 save , stats 에 맞춰서 구현
bool save_is_plant_completed(const char* plant_id);
int save_get_plant_logs(const char* plant_id, CodexLog* out, int max);

// --- 내부 ----
typedef struct {
	int unlocked;
	SDL_Texture* thumb; // 해금 썸네일
} CodexEntry;

static CodexEntry* s_entries = NULL;
static int		   s_count = 0;

static SDL_Texture* s_locked = NULL;
static int s_scroll = 0;
static int s_selected = -1;
static SDL_Rect s_btnBack;
static TTF_Font* s_font = NULL;

// 그리드 레이아웃
enum { CELL = 128, GAP = 18, COLS = 5};
static int grid_origin_x = 56;
static int grid_origin_y = 130;

//유틸리티
static SDL_Texture* load_tex(const char* path) {
	SDL_Texture* t = IMG_LoadTexture(G_Renderer, path);
	if (!t) SDL_Log("Codex: IMG load fail %s : %s",path, IMG_GetError());
	return t;
}

static void draw_text(SDL_Renderer* r, TTF_Font* f, int x, int y, const char* s, SDL_Color c) {
	if (!f || !s) return;
	SDL_Surface* sf = TTF_RenderUTF8_Blended(f, s, c);
	if (!sf) return;
	SDL_Texture* tx = SDL_CreateTextureFromSurface(r, sf);
	SDL_Rect d = { x,y, sf->w, sf->h };
	SDL_FreeSurface(sf);
	SDL_RenderCopy(r, tx, NULL, &d);
	SDL_DestroyTexture(tx);
}

static void layout(int win_w, int win_h) {
	(void)win_h;
	s_btnBack = (SDL_Rect){ win_w - 120 - 20 , 20 ,120 , 40 };
}

//초기화

static void init(void) {
	int w, h;
	SDL_GetRendererOutputSize(G_Renderer, &w, &h);
	layout(w, h);

	if (!s_font) {
		s_font = TTF_OpenFont(ASSETS_FONTS_DIR "NotoSansKR.ttf", 22);
		if (!s_font) SDL_Log("CODEX 폰트로드 실패:%s", TTF_GetError());
	}

	//데이터 수집
	s_count = plantdb_count();
	s_entries = (CodexEntry*)SDL_calloc(s_count, sizeof(CodexEntry));
	if (!s_entries) { SDL_Log("CODEX: alloc fail"); return; }
	
	//공용잠금 텍스처
	s_locked = load_tex(ASSETS_IMAGES_DIR "codex/locked_collection.png");

	//각 식물별 썸네일 로드
	for (int i = 0; i < s_count; i++) {
		const PlantInfo* p = plantdb_get(i);
		if (!p) continue;
		s_entries[i].unlocked = save_is_plant_completed(p->id) ? 1 : 0;

		if (s_entries[i].unlocked) {
			char path[512];
			SDL_snprintf(path, sizeof(path), ASSETS_IMAGES_DIR "codex/%s_thumb.png", p->id);
			s_entries[i].thumb = load_tex(path);
		}
		else {
			s_entries[i].thumb = NULL;
		}
	}

	s_scroll = 0;
	s_selected = -1;

}

//이벤트 처리

static void handle(SDL_Event* e) {
	if (e->type == SDL_QUIT) { G_Running = 0; return; }

	if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
		layout(e->window.data1, e->window.data2);
	}

	if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
		scene_switch(SCENE_MAINMENU);
		return;
	}

	if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
		scene_switch(SCENE_MAINMENU);
		return;
	}

	if (e->type == SDL_MOUSEWHEEL) {
		// 스크롤: 위로 양수, 아래 음수(SDL은 반대일 수 있으니 체감으로 조절)
		s_scroll -= e->wheel.y * (CELL + GAP) / 2;
		if (s_scroll < 0) s_scroll = 0;
	}

	if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
		int mx = e->button.x, my = e->button.y;

		if (mx >= s_btnBack.x && my >= s_btnBack.y && mx < s_btnBack.x + s_btnBack.w && s_btnBack.y + s_btnBack.h) {
			scene_switch(SCENE_MAINMENU);
			return;
		}
		//그리드 히트
		int x0 = grid_origin_x;
		int y0 = grid_origin_y - s_scroll;
		for (int i = 0;i < s_count;i++) {
			int row = i / COLS;
			int col = i % COLS;
			SDL_Rect rc = { x0 + col * (CELL + GAP), y0 + row * (CELL + GAP), CELL, CELL };
			if (mx >= rc.x && my >= rc.y && mx < rc.x + rc.w && my < rc.y + rc.h) {
				s_selected = i;
				break;
			}
		}
	}
}

static void update(float dt) {
	(void)dt;
}

static void render(SDL_Renderer* r) {
	int w, h;
	SDL_GetRendererOutputSize(r, &w, &h);

	//배경
	SDL_SetRenderDrawColor(r, 16, 20, 28, 255);
	SDL_RenderClear(r);

	//타이틀
	draw_text(r, s_font, 36, 26, "도감", (SDL_Color) { 230, 240, 235, 255 });

	//뒤로가기 버튼
	SDL_SetRenderDrawColor(r, 60, 120, 200, 255);
	SDL_RenderFillRect(r, &s_btnBack);
	draw_text(r, s_font, s_btnBack.x + 24, s_btnBack.y + 8, "뒤로", (SDL_Color) { 245, 245, 245, 255 });

	//그리드
	int x0 = grid_origin_x;
	int y0 = grid_origin_y - s_scroll;
	for (int i = 0; i < s_count; i++) {
		int row = i / COLS;
		int col = i % COLS;
		SDL_Rect cell = { x0 + col * (CELL + GAP), y0 + row * (CELL + GAP),CELL,CELL };

		//셀 배경
		SDL_SetRenderDrawColor(r, 28, 36, 48, 255);
		SDL_RenderFillRect(r, &cell);
		SDL_SetRenderDrawColor(r, 200, 210, 220, 100);
		SDL_RenderFillRect(r, &cell);

		//썸네일 or 잠금
		SDL_Texture* tex = s_entries[i].unlocked ? s_entries[i].thumb : s_locked;
		if (tex) {
			//정사각형 중앙 배열
			int tw, th;
			SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
			float scale = (float)CELL / (float)(tw > th ? tw : th);
			int rw = (int)(tw * scale);
			int rh = (int)(th * scale);
			SDL_Rect dst = { cell.x + (CELL - rw) / 2,cell.y + (CELL - rh) / 2,rw,rh };
			SDL_RenderCopy(r, tex, NULL, &dst);
		}

		//이름 해금된거
		if (s_entries[i].unlocked && s_font) {
			const PlantInfo* p = plantdb_get(i);
			if (p) {
				SDL_Color c = { 210,220,230,220 };
				draw_text(r, s_font, cell.x, cell.y + CELL + 6, p->name_kr, c);
			}
		}

		//선택 하이라이트
		if (i == s_selected) {
			SDL_SetRenderDrawColor(r, 255, 230, 120, 180);
			SDL_RenderDrawRect(r, &cell);
		}
	}

	SDL_Rect panel = { w - 360,90,320,h - 120 };
	SDL_SetRenderDrawColor(r, 30, 42, 62, 255);
	SDL_RenderFillRect(r, &panel);
	SDL_SetRenderDrawColor(r, 200, 210, 220, 120);
	SDL_RenderFillRect(r, &panel);

	if (s_selected >= 0 && s_selected < s_count) {
		const PlantInfo* p = plantdb_get(s_selected);
		if (p) {
			//제목
			draw_text(r, s_font, panel.x + 16, panel.y + 12, p->name_kr, (SDL_Color){ 235,245,240,255 });
			//큰 이미지(해금 시)
			if (s_entries[s_selected].unlocked) {
				char path[512];
				SDL_snprintf(path, sizeof(path), ASSETS_IMAGES_DIR "codex/%s_ful.png", p->id);
				SDL_Texture* full = load_tex(path);
				if (full) {
					int tw, th;
					SDL_QueryTexture(full, NULL, NULL, &tw, &th);
					int maxW = panel.w - 32, maxH = 160;
					float s = (float)maxW / (float)tw;
					if (th * s > maxH) s = (float)maxH / (float)th;
					int rw = (int)(tw * s), rh = (int)(th * s);
					SDL_Rect d = { panel.x + 16, panel.y + 48,rw,rh };
					SDL_RenderCopy( r,full,NULL,&d );
					SDL_DestroyTexture(full);
				}
			}
			else {
				draw_text(r, s_font, panel.x + 16, panel.y + 48, "미해금 - 꽃을 피워보세요", (SDL_Color) { 220, 120, 120, 255 });
			}
			//기록 목록(해금)
			if (s_entries[s_selected].unlocked) {
				CodexLog logs[64];
				int n = save_get_plant_logs(p->id, logs, 64);
				int yy = panel.y + 48 + 180;
				draw_text(r, s_font, panel.x + 16, yy, "기록", (SDL_Color) { 210, 240, 220, 255 });
				yy += 28;
				for (int i = 0; i < n && yy < panel.y + panel.w; ++i) {
					draw_text(r, s_font, panel.x + 16, yy, logs[i].line, (SDL_Color) { 200, 210, 220, 220 });
					yy += 22;
				}
			}
		}
	}
	SDL_RenderPresent(r);

}

static void cleanup(void) {
	if (s_entries) {
		for (int i = 0; i < s_count; i++) {
			if (s_entries[i].thumb) SDL_DestroyTexture(s_entries[i].thumb);
		}
		SDL_free(s_entries); s_entries = NULL;
	}
	if (s_locked) { SDL_DestroyTexture(s_locked); s_locked = NULL; }
}

//씬 객체
static Scene SCENE_OBJ = { init,handle,update,render,cleanup," Codex" };
Scene* scene_codex_object(void) { return &SCENE_OBJ; }