import asyncio
import threading
from flask import Flask
from bleak import BleakClient, BleakScanner
import joblib
import numpy as np
import hangul_jamo

app = Flask(__name__)
lock = threading.Lock()
current_char = ""  # BLE 장치 정보
TARGET_NAME = "FlexIMUGlove"
CHAR_UUID = "2A99"
model = joblib.load("knn_model_no_scaling.pkl")

# 겹자음 (초성용)
DOUBLE_CONSONANTS = {
    ('ㄱ', 'ㄱ'): 'ㄲ',
    ('ㄷ', 'ㄷ'): 'ㄸ',
    ('ㅂ', 'ㅂ'): 'ㅃ',
    ('ㅅ', 'ㅅ'): 'ㅆ',
    ('ㅈ', 'ㅈ'): 'ㅉ',
}
DOUBLE_TO_SINGLE = {v: k for k, v in DOUBLE_CONSONANTS.items()}

# 겹받침 (종성용)
COMPLEX_FINALS = {
    ('ㄱ', 'ㅅ'): 'ㄳ',
    ('ㄴ', 'ㅈ'): 'ㄵ',
    ('ㄴ', 'ㅎ'): 'ㄶ',
    ('ㄹ', 'ㄱ'): 'ㄺ',
    ('ㄹ', 'ㅁ'): 'ㄻ',
    ('ㄹ', 'ㅍ'): 'ㄿ',
    ('ㄹ', 'ㅎ'): 'ㅀ',
    ('ㅂ', 'ㅅ'): 'ㅄ',
}
REVERSE_COMPLEX_FINALS = {v: k for k, v in COMPLEX_FINALS.items()}


def is_vowel(char):
    return char in 'ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅛㅜㅠㅡㅣ'


def is_consonant(char):
    return not is_vowel(char)


# 상태 버퍼
buffer = []
pending_double = None
pending_final = None


def parse_ble_string(data_str):
    try:
        parts = data_str.strip().split(";")
        flex = list(map(int, parts[0].split(":")[1].split(",")))
        mag = list(map(float, parts[1].split(":")[1].split(",")))
        return flex + mag
    except Exception as e:
        print("⚠파싱\n오류:", e)
        return None


def handle_notify(_, data):
    global current_char, buffer, pending_double, pending_final
    try:
        text = data.decode("utf-8").strip()
        print(f"\n수신 문자열: {text}")
        parsed = parse_ble_string(text)
        if parsed and len(parsed) == 8:
            prediction = model.predict([parsed])[0]
            print(f"\n예측된 자모: {prediction}")
            with lock:
                if prediction == "0":
                    # 처리되지 않은 겹자음 → 결합
                    if pending_double:
                        if buffer and is_consonant(buffer[-1]):
                            prev = buffer.pop()
                            combined = DOUBLE_CONSONANTS.get((prev, pending_double))
                            if combined:
                                buffer.append(combined)
                            else:
                                buffer.append(prev)
                                buffer.append(pending_double)
                        else:
                            buffer.append(pending_double)
                        pending_double = None
                    # 처리되지 않은 겹받침 → 결합
                    if pending_final:
                        if buffer and is_consonant(buffer[-1]):
                            prev = buffer.pop()
                            combined = COMPLEX_FINALS.get((prev, pending_final))
                            if combined:
                                buffer.append(combined)
                            else:
                                buffer.append(prev)
                                buffer.append(pending_final)
                        else:
                            buffer.append(pending_final)
                        pending_final = None
                    try:
                        composed = hangul_jamo.compose(''.join(buffer))
                        print(f"\n조합된 글자: {composed}")
                        current_char = composed
                    except Exception as e:
                        print("❌ 조합 실패:", e)
                        current_char = "[조합 오류]"
                        buffer.clear()
                elif prediction == "1":
                    # 되돌리기
                    if pending_double:
                        pending_double = None
                    elif pending_final:
                        pending_final = None
                    elif buffer:
                        buffer.pop()
                    current_char = ''.join(buffer) if buffer else ""
                else:
                    char = prediction
                    if pending_double:
                        if is_vowel(char):
                            buffer.append(pending_double)
                        else:
                            prev = buffer.pop()
                            combined = DOUBLE_CONSONANTS.get((prev, pending_double))
                            if combined:
                                buffer.append(combined)
                            else:
                                buffer.append(prev)
                                buffer.append(pending_double)
                        pending_double = None
                    elif pending_final:
                        if is_vowel(char):
                            buffer.append(pending_final)
                        else:
                            prev = buffer.pop()
                            combined = COMPLEX_FINALS.get((prev, pending_final))
                            if combined:
                                buffer.append(combined)
                            else:
                                buffer.append(prev)
                                buffer.append(pending_final)
                        pending_final = None

                    if buffer and is_consonant(char) and buffer[-1] == char:
                        pending_double = char
                    elif buffer and is_consonant(char):
                        prev = buffer[-1]
                        if (prev, char) in COMPLEX_FINALS:
                            pending_final = char
                        else:
                            buffer.append(char)
                    else:
                        buffer.append(char)
                    # 실시간 자모 상태 표시
                    current_char = ''.join(buffer)
        else:
            print("⚠예측\n입력 데이터 길이 오류")
    except Exception as e:
        print("❌ BLE 처리 중 오류:", e)


async def ble_main():
    print("\nBLE 장치 검색 중...")
    devices = await BleakScanner.discover()
    device = next((d for d in devices if d.name == TARGET_NAME), None)
    if not device:
        print("❌ 장치를 찾지 못했습니다.")
        return
    print(f"✅ 장치 연결됨: {device.name} [{device.address}]")
    async with BleakClient(device.address) as client:
        await asyncio.sleep(1.0)
        await client.start_notify(CHAR_UUID, handle_notify)
        print("\n수신 대기 중..")
        try:
            while True:
                await asyncio.sleep(1)
        except asyncio.CancelledError:
            await client.stop_notify(CHAR_UUID)
            print("BLE\n종료됨")


def start_ble_loop():
    asyncio.run(ble_main())


@app.route('/get_result')
def get_result():
    with lock:
        return current_char


if __name__ == '__main__':
    ble_thread = threading.Thread(target=start_ble_loop)
    ble_thread.daemon = True
    ble_thread.start()
    app.run(host='0.0.0.0', port=5000)
