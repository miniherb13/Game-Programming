#!/usr/bin/env python3
"""메인 통합 루프 — A 담당"""

import subprocess
import sys
import threading
import time

import cv2

from config import CAPTURE_INTERVAL, CAPTURE_PATH
from lcd_actuator import Actuator
from motion import detect_motion
from reply import get_reply


def capture(path: str) -> None:
    subprocess.run(
        ["rpicam-still", "-n", "-t", "1000", "-o", path],
        check=True,
    )


def input_thread(act: Actuator, stop: threading.Event) -> None:
    while not stop.is_set():
        try:
            text = input("\n[대화] 질문 입력: ").strip()
        except EOFError:
            break
        if not text:
            continue
        line1, line2 = get_reply(text)
        act.show_reply(line1, line2)


def main() -> None:
    act = Actuator()
    stop = threading.Event()
    t = threading.Thread(target=input_thread, args=(act, stop), daemon=True)
    t.start()

    prev = None
    try:
        act.show_idle()
        while True:
            capture(CAPTURE_PATH)
            curr = cv2.imread(CAPTURE_PATH)
            if curr is None:
                print(f"Cannot read {CAPTURE_PATH}")
                time.sleep(CAPTURE_INTERVAL)
                continue

            if prev is not None and detect_motion(prev, curr):
                act.show_motion()
            else:
                act.show_idle()

            prev = curr
            time.sleep(CAPTURE_INTERVAL)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        stop.set()
        act.close()


if __name__ == "__main__":
    main()
