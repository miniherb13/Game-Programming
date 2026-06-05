#!/usr/bin/env python3
"""메인 통합 루프 — A 담당 (카메라 motion + 초음파 + 대화)."""

import subprocess
import threading
import time

import cv2

from config import (
    CAPTURE_INTERVAL,
    CAPTURE_PATH,
    DEBUG_MOTION,
    MOTION_COOLDOWN,
    MOTION_THRESHOLD,
    ULTRASONIC_CLOSE_CM,
)
from lcd_actuator import Actuator
from motion import detect_motion, motion_score
from reply import get_reply


def capture(path: str) -> bool:
    try:
        subprocess.run(
            ["rpicam-still", "-n", "-t", "1000", "-o", path],
            check=True,
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Capture failed: {e}")
        return False


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


def is_close(distance: float) -> bool:
    return distance != -1 and distance <= ULTRASONIC_CLOSE_CM


def main() -> None:
    act = Actuator()
    stop = threading.Event()
    t = threading.Thread(target=input_thread, args=(act, stop), daemon=True)
    t.start()

    prev = None
    last_motion_at = 0.0

    try:
        act.show_idle()
        print("Companion bot running.")
        print("- Camera motion OR ultrasonic <=", ULTRASONIC_CLOSE_CM, "cm")
        print("- Type a question in terminal")
        print("Ctrl+C to stop.\n")

        while True:
            distance = act.read_distance()
            print(f"[ultrasonic] {distance} cm")

            if not capture(CAPTURE_PATH):
                time.sleep(CAPTURE_INTERVAL)
                continue

            curr = cv2.imread(CAPTURE_PATH)
            if curr is None:
                print(f"Cannot read {CAPTURE_PATH}")
                time.sleep(CAPTURE_INTERVAL)
                continue

            camera_motion = prev is not None and detect_motion(prev, curr)
            triggered = camera_motion or is_close(distance)

            now = time.monotonic()
            if triggered:
                if DEBUG_MOTION and camera_motion:
                    score = motion_score(prev, curr)
                    print(f"[motion] score={score:.1f} threshold={MOTION_THRESHOLD}")

                if now - last_motion_at >= MOTION_COOLDOWN:
                    act.show_motion(distance)
                    last_motion_at = now
            else:
                act.show_idle(distance)

            prev = curr
            time.sleep(CAPTURE_INTERVAL)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        stop.set()
        act.close()


if __name__ == "__main__":
    main()
