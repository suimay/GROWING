#ifndef LOADING_H
#define LOADING_H
#include "../include/scene.h"

// 로딩 작업 콜백 (옵션): 오래 걸리는 리소스 로드를 여기서 수행
typedef void (*LoadingJobFn)(void* userdata);

// 로딩 시작: 로딩씬으로 진입 → (job 수행) → 완료되면 target으로 자동 전환
void loading_begin(SceneID target, LoadingJobFn job, void* userdata,
    float fade_out_sec, float fade_in_sec);

// 로딩씬 객체 (main에서 등록 필요)
Scene* scene_loading_object(void);

#endif
