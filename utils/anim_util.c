// anim_util.c
#include "../include/anim_util.h"
#include <SDL2/SDL_image.h>
#include <parson.h>

/// pngPath / jsonPath 를 기반으로
///  - atlas 텍스처를 로드하고(outAtlas)
///  - JSON을 파싱해서 프레임 정보(outFrames)에 채운다.
/// return: 0 성공 / !=0 실패
int anim_load_from_json(
    SDL_Renderer* renderer,
    const char* pngPath,
    const char* jsonPath,
    plantFrame* outFrames,
    int maxFrames,
    SDL_Texture** outAtlas,
    int* outFrameCount)
{
    if (!renderer || !pngPath || !jsonPath ||
        !outFrames || maxFrames <= 0 || !outAtlas || !outFrameCount)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[ANIM] invalid argument");
        return -1;
    }

    *outAtlas = NULL;
    *outFrameCount = 0;

    // 1) 텍스처 로드
    SDL_Texture* atlas = IMG_LoadTexture(renderer, pngPath);
    if (!atlas) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[ANIM] IMG_LoadTexture fail %s : %s",
            pngPath, IMG_GetError());
        return -2;
    }

    int atlasW = 0, atlasH = 0;
    SDL_QueryTexture(atlas, NULL, NULL, &atlasW, &atlasH);

    // 2) JSON 파싱
    JSON_Value* root = json_parse_file(jsonPath);
    if (!root) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[ANIM] json parse fail %s", jsonPath);
        SDL_DestroyTexture(atlas);
        return -3;
    }

    JSON_Object* robj = json_value_get_object(root);
    if (!robj) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[ANIM] json root is not object: %s", jsonPath);
        json_value_free(root);
        SDL_DestroyTexture(atlas);
        return -4;
    }

    JSON_Object* framesObj = json_object_get_object(robj, "frames");
    JSON_Array* framesArr = json_object_get_array(robj, "frames");

    int frameCount = 0;

    // --------------------------------------------------
    // 3-A) frames 가 object 인 경우 (키: 프레임 이름)
    // --------------------------------------------------
    if (framesObj) {
        size_t count = json_object_get_count(framesObj);
        SDL_Log("[ANIM] frames=object, count=%zu", count);

        for (size_t idx = 0;
            idx < count && frameCount < maxFrames;
            ++idx)
        {
            const char* key = json_object_get_name(framesObj, idx);
            JSON_Object* fobj = json_object_get_object(framesObj, key);
            if (!fobj) continue;

            JSON_Object* rectObj = json_object_get_object(fobj, "frame");
            if (!rectObj) {
                SDL_Log("[ANIM] missing frame for key=%s", key);
                continue;
            }

            plantFrame fr = { 0 };
            fr.rect.x = (int)json_object_get_number(rectObj, "x");
            fr.rect.y = (int)json_object_get_number(rectObj, "y");
            fr.rect.w = (int)json_object_get_number(rectObj, "w");
            fr.rect.h = (int)json_object_get_number(rectObj, "h");
            fr.duration_ms = (int)json_object_get_number(fobj, "duration");
            if (fr.duration_ms <= 0) fr.duration_ms = 100;

            // 안전 체크 (옵션)
            if (fr.rect.w <= 0 || fr.rect.h <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "[ANIM] invalid WH key=%s", key);
                continue;
            }
            if (atlasW > 0 && atlasH > 0) {
                if (fr.rect.x < 0 || fr.rect.y < 0 ||
                    fr.rect.x + fr.rect.w > atlasW ||
                    fr.rect.y + fr.rect.h > atlasH)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "[ANIM] OOB key=%s rect=%d,%d,%d,%d atlas=%d,%d",
                        key, fr.rect.x, fr.rect.y, fr.rect.w, fr.rect.h,
                        atlasW, atlasH);
                    continue;
                }
            }

            outFrames[frameCount++] = fr;
        }
    }
    // --------------------------------------------------
    // 3-B) frames 가 array 인 경우
    // --------------------------------------------------
    else if (framesArr) {
        size_t count = json_array_get_count(framesArr);
        SDL_Log("[ANIM] frames=array, count=%zu", count);

        for (size_t idx = 0;
            idx < count && frameCount < maxFrames;
            ++idx)
        {
            JSON_Object* fobj = json_array_get_object(framesArr, idx);
            if (!fobj) continue;

            JSON_Object* rectObj = json_object_get_object(fobj, "frame");
            if (!rectObj) {
                SDL_Log("[ANIM] missing frame at %zu", idx);
                continue;
            }

            plantFrame fr = { 0 };
            fr.rect.x = (int)json_object_get_number(rectObj, "x");
            fr.rect.y = (int)json_object_get_number(rectObj, "y");
            fr.rect.w = (int)json_object_get_number(rectObj, "w");
            fr.rect.h = (int)json_object_get_number(rectObj, "h");
            fr.duration_ms = (int)json_object_get_number(fobj, "duration");
            if (fr.duration_ms <= 0) fr.duration_ms = 100;

            if (fr.rect.w <= 0 || fr.rect.h <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "[ANIM] invalid WH idx=%zu", idx);
                continue;
            }
            if (atlasW > 0 && atlasH > 0) {
                if (fr.rect.x < 0 || fr.rect.y < 0 ||
                    fr.rect.x + fr.rect.w > atlasW ||
                    fr.rect.y + fr.rect.h > atlasH)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "[ANIM] OOB idx=%zu rect=%d,%d,%d,%d atlas=%d,%d",
                        idx, fr.rect.x, fr.rect.y, fr.rect.w, fr.rect.h,
                        atlasW, atlasH);
                    continue;
                }
            }

            outFrames[frameCount++] = fr;
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[ANIM] no frames object/array in %s", jsonPath);
        json_value_free(root);
        SDL_DestroyTexture(atlas);
        return -5;
    }

    json_value_free(root);

    if (frameCount <= 0) {
        SDL_Log("[ANIM] parsed but no valid frames from %s", jsonPath);
        SDL_DestroyTexture(atlas);
        return -6;
    }

    *outAtlas = atlas;
    *outFrameCount = frameCount;

    SDL_Log("[ANIM] loaded %d frames from %s (atlas=%p, %dx%d)",
        frameCount, jsonPath, (void*)atlas, atlasW, atlasH);

    return 0;
}


