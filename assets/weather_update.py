import requests
from datetime import datetime, timezone, timedelta

# ====== 여기만 네 환경에 맞게 수정 ======
API_KEY = "9d2280e95c6d673406795ecc6080e513"
LAT = 37.5665   # 위도 (서울 예시)
LON = 126.9780  # 경도 (서울 예시)
OUTPUT_FILE = "assets/weather_state.txt"
# =====================================

# 한국 표준시 (UTC+9)
KST = timezone(timedelta(hours=9))


def map_weather_tag(main_str: str) -> str:
    """
    OpenWeatherMap 의 weather[0].main 문자열을
    게임에서 쓸 간단한 태그로 변환.
    """
    if not main_str:
        return "UNKNOWN"

    m = main_str.lower()

    if m == "clear":
        return "CLEAR"
    elif m in ("clouds", "mist", "haze", "fog", "smoke"):
        return "CLOUDY"
    elif m in ("rain", "drizzle"):
        return "RAIN"
    elif m == "snow":
        return "SNOW"
    elif m in ("thunderstorm", "tornado", "squall"):
        return "STORM"
    else:
        return "UNKNOWN"


def fetch_weather() -> dict:
    url = "https://api.openweathermap.org/data/2.5/weather"

    params = {
        "lat": LAT,
        "lon": LON,
        "appid": API_KEY,
        "units": "metric",
    }

    # 타임아웃 없으면 멈춘 것처럼 느껴질 수 있어서 5초 정도 줌
    resp = requests.get(url, params=params, timeout=5)
    resp.raise_for_status()

    data = resp.json()

    # 문서에 보장된 필드만 사용
    weather_main = data["weather"][0]["main"]      # 예: "Clear", "Clouds", "Rain"
    temp = float(data["main"]["temp"])             # 현재 기온(°C)
    humidity = int(data["main"]["humidity"])       # 습도 (%)

    sunrise_ts = int(data["sys"]["sunrise"])       # Unix time (초)
    sunset_ts  = int(data["sys"]["sunset"])

    sunrise_dt = datetime.fromtimestamp(sunrise_ts, tz=KST)
    sunset_dt  = datetime.fromtimestamp(sunset_ts, tz=KST)

    time_fmt = "%Y-%m-%d %H:%M:%S"
    sunrise_str = sunrise_dt.strftime(time_fmt)
    sunset_str  = sunset_dt.strftime(time_fmt)

    tag = map_weather_tag(weather_main)

    return {
        "tag": tag,
        "temp": temp,
        "humidity": humidity,
        "raw_main": weather_main,
        "sunrise_ts": sunrise_ts,
        "sunset_ts": sunset_ts,
        "sunrise_str": sunrise_str,
        "sunset_str": sunset_str,
    }


def save_weather(info: dict) -> None:
    """
    게임 쪽(C)에서 읽기 쉬운 단순 텍스트 포맷으로 저장.
    """
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        # 태그/수치 정보
        f.write(f"TAG={info['tag']}\n")
        # 온도는 소수 1~2자리 정도만 쓰고 싶으면 format 조정 가능
        f.write(f"TEMP={info['temp']:.2f}\n")
        f.write(f"HUMIDITY={info['humidity']}\n")
        f.write(f"RAW={info['raw_main']}\n")

        # 타임스탬프(정수)
        f.write(f"SUNRISE_TS={info['sunrise_ts']}\n")
        f.write(f"SUNSET_TS={info['sunset_ts']}\n")

        # ✨ 문자열로 확실히 보이게 하기 위해 따옴표로 감쌈
        # C 쪽에서는 line + 8, line + 7로 읽으면 "2025-..."까지 그대로 들어감
        f.write(f"SUNRISE=\"{info['sunrise_str']}\"\n")
        f.write(f"SUNSET=\"{info['sunset_str']}\"\n")


def main():
    try:
        info = fetch_weather()
    except requests.RequestException as e:
        print("날씨 가져오기 실패:", e)
        # 실패했을 때도 게임이 죽지 않게 기본값 한 번 저장
        info = {
            "tag": "UNKNOWN",
            "temp": 0.0,
            "humidity": 0,
            "raw_main": "ERROR",
            "sunrise_ts": 0,
            "sunset_ts": 0,
            "sunrise_str": "2025-01-01 00:00:00",
            "sunset_str": "2025-01-01 00:00:00",
        }

    print("현재 날씨 정보:")
    print("  태그 :", info["tag"])
    print("  원본 :", info["raw_main"])
    print("  온도 :", info["temp"])
    print("  습도 :", info["humidity"])
    print("  일출(KST) :", info["sunrise_str"])
    print("  일몰(KST) :", info["sunset_str"])

    save_weather(info)
    print(f"→ {OUTPUT_FILE} 파일에 저장 완료")


if __name__ == "__main__":
    main()
