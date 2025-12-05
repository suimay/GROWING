#pragma once
#ifndef WEATHER_H
#define WEATHER_H

typedef enum {
    WEATHER_TAG_UNKNOWN = 0,
    WEATHER_TAG_CLEAR,
    WEATHER_TAG_CLOUDY,
    WEATHER_TAG_RAIN,
    WEATHER_TAG_SNOW,
    WEATHER_TAG_STORM
} WeatherTag;

typedef enum {
    TIMEOFDAY_NIGHT = 0,
    TIMEOFDAY_SUNRISE,
    TIMEOFDAY_DAY,
    TIMEOFDAY_SUNSET
} TimeOfDay;

typedef struct WeatherInfo {
    WeatherTag tag;
    double temp;
    int humidity;

    char raw_weather[32];

    // 일출/일몰 문자열 (KST)
    char sunrise[32];
    char sunset[32];

    // 필요하면 timestamp로도 저장
    long sunrise_ts;
    long sunset_ts;

    int sunrise_sec;
    int sunset_sec;

} WeatherInfo;

// 외부에서 사용할 함수
int weather_load(struct WeatherInfo* out);

// 일출/일몰 문자열을 파싱해서 sunrise_sec / sunset_sec 채우는 함수
void weather_compute_sun_secs(struct WeatherInfo* w);

// 현재 시각 + 일출/일몰 정보를 바탕으로 시간대 구하기
TimeOfDay weather_get_time_of_day(const struct WeatherInfo* w);

#endif
