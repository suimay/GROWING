#define _CRT_SECURE_NO_WARNINGS
#include "../include/weather.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define WEATHER_FILE_PATH "assets/weather_state.txt"

// 문자열 → enum 변환
static WeatherTag map_tag(const char* t) {
    if (strcmp(t, "CLEAR") == 0) return WEATHER_TAG_CLEAR;
    if (strcmp(t, "CLOUDY") == 0) return WEATHER_TAG_CLOUDY;
    if (strcmp(t, "RAIN") == 0) return WEATHER_TAG_RAIN;
    if (strcmp(t, "SNOW") == 0) return WEATHER_TAG_SNOW;
    if (strcmp(t, "STORM") == 0) return WEATHER_TAG_STORM;
    return WEATHER_TAG_UNKNOWN;
}

// 파일 로드
int weather_load(struct WeatherInfo* out) {
    FILE* fp = fopen(WEATHER_FILE_PATH, "r");
    if (!fp) {
        printf("날씨 파일 열기 실패: %s\n", WEATHER_FILE_PATH);
        return 0;
    }

    char line[256];

    // 기본값 초기화
    memset(out, 0, sizeof(*out));
    out->tag = WEATHER_TAG_UNKNOWN;

    while (fgets(line, sizeof(line), fp)) {
        // 개행 제거
        line[strcspn(line, "\r\n")] = '\0';

        // TAG
        if (strncmp(line, "TAG=", 4) == 0) {
            out->tag = map_tag(line + 4);
        }
        // TEMP
        else if (strncmp(line, "TEMP=", 5) == 0) {
            out->temp = atof(line + 5);
        }
        // HUMIDITY
        else if (strncmp(line, "HUMIDITY=", 9) == 0) {
            out->humidity = atoi(line + 9);
        }
        // RAW
        else if (strncmp(line, "RAW=", 4) == 0) {
            strncpy(out->raw_weather, line + 4, sizeof(out->raw_weather) - 1);
        }
        // SUNRISE (문자열)
        else if (strncmp(line, "SUNRISE=", 8) == 0) {
            strncpy(out->sunrise, line + 8, sizeof(out->sunrise) - 1);
        }
        // SUNSET (문자열)
        else if (strncmp(line, "SUNSET=", 7) == 0) {
            strncpy(out->sunset, line + 7, sizeof(out->sunset) - 1);
        }
        // SUNRISE_TS
        else if (strncmp(line, "SUNRISE_TS=", 12) == 0) {
            out->sunrise_ts = atol(line + 12);
        }
        // SUNSET_TS
        else if (strncmp(line, "SUNSET_TS=", 11) == 0) {
            out->sunset_ts = atol(line + 11);
        }
    }

    fclose(fp);
    return 1;
}


void weather_compute_sun_secs(struct WeatherInfo* w) {
    int y, M, d, h, m, s;

    // 기본값
    w->sunrise_sec = 6 * 3600;   // 06:00 (파싱 실패시 대비용, 설계 선택)
    w->sunset_sec = 18 * 3600;  // 18:00

    // SUNRISE="YYYY-MM-DD HH:MM:SS"
    if (sscanf(w->sunrise, "%d-%d-%d %d:%d:%d", &y, &M, &d, &h, &m, &s) == 6) {
        w->sunrise_sec = h * 3600 + m * 60 + s;
    }

    if (sscanf(w->sunset, "%d-%d-%d %d:%d:%d", &y, &M, &d, &h, &m, &s) == 6) {
        w->sunset_sec = h * 3600 + m * 60 + s;
    }
}

// 현재 로컬 시간(KST 기준이라고 가정)에서 시간대 계산
TimeOfDay weather_get_time_of_day(const struct WeatherInfo* w) {
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);   // 로컬 시간 (윈도우 KST면 그대로 KST)

    if (!lt) {
        return TIMEOFDAY_DAY; // 실패 시 기본값 (설계 선택)
    }

    int cur_sec = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;

    // 구간 폭은 "게임 느낌"에 맞춰 적당히 정한 거라 설계 선택 (추측이 아니라 디자인)
    const int SUNRISE_BEFORE = 30 * 60;   // 일출 30분 전까지는 밤
    const int SUNRISE_AFTER = 60 * 60;   // 일출 후 1시간까지는 일출 구간
    const int SUNSET_BEFORE = 60 * 60;   // 일몰 1시간 전부터 일몰 구간 시작
    const int SUNSET_AFTER = 30 * 60;   // 일몰 후 30분까지 일몰 구간 계속

    int sunrise_start = w->sunrise_sec - SUNRISE_BEFORE;
    int sunrise_end = w->sunrise_sec + SUNRISE_AFTER;
    int sunset_start = w->sunset_sec - SUNSET_BEFORE;
    int sunset_end = w->sunset_sec + SUNSET_AFTER;

    // 범위 보정 (0~24h 사이 유지)
    if (sunrise_start < 0) sunrise_start = 0;
    if (sunset_end > 24 * 3600) sunset_end = 24 * 3600;

    if (cur_sec < sunrise_start) {
        return TIMEOFDAY_NIGHT;
    }
    else if (cur_sec < sunrise_end) {
        return TIMEOFDAY_SUNRISE;
    }
    else if (cur_sec < sunset_start) {
        return TIMEOFDAY_DAY;
    }
    else if (cur_sec < sunset_end) {
        return TIMEOFDAY_SUNSET;
    }
    else {
        return TIMEOFDAY_NIGHT;
    }
}