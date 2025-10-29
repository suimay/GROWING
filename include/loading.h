#pragma once
#include "../include/scene.h"

typedef int (*LoadingJobFn)(void* userdata, float* out_progress); // 0~100

typedef struct {
    SceneID      target;
    LoadingJobFn job;
    void* userdata;
    float        progress;   // 0~100
    int          done;       // 0/1
} LoadingData;

extern LoadingData LD;

void loading_begin(SceneID target, LoadingJobFn job, void* userdata,
    float fade_out_sec, float fade_in_sec);

Scene* scene_loading_object(void);
