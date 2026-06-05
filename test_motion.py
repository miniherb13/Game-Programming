#!/usr/bin/env python3
"""Motion-only test.

Pi camera:
  python3 test_motion.py live

PC two images:
  python3 test_motion.py still prev.jpg curr.jpg
"""

import subprocess
import sys
import time

import cv2

from config import CAPTURE_INTERVAL, CAPTURE_PATH, DEBUG_MOTION, MOTION_THRESHOLD
from motion import detect_motion, motion_score


def capture(path: str) -> None:
    subprocess.run(
        ["rpicam-still", "-n", "-t", "1000", "-o", path],
        check=True,
    )


def run_live() -> None:
    prev = None
    print(f"Motion live test - threshold={MOTION_THRESHOLD}, interval={CAPTURE_INTERVAL}s")
    print("Ctrl+C to stop.\n")
    try:
        while True:
            try:
                capture(CAPTURE_PATH)
            except (subprocess.CalledProcessError, FileNotFoundError) as e:
                print(f"Capture failed: {e}")
                print("Check rpicam-still and camera on Pi.")
                break

            curr = cv2.imread(CAPTURE_PATH)
            if curr is None:
                print(f"Cannot read {CAPTURE_PATH}")
                time.sleep(CAPTURE_INTERVAL)
                continue

            if prev is not None:
                score = motion_score(prev, curr)
                moved = score > MOTION_THRESHOLD
                if DEBUG_MOTION or moved:
                    print(f"motion={moved}  score={score:.1f}  (threshold={MOTION_THRESHOLD})")
                else:
                    print(f"motion={moved}  score={score:.1f}")

            prev = curr
            time.sleep(CAPTURE_INTERVAL)
    except KeyboardInterrupt:
        print("\nStopped.")


def run_still(prev_path: str, curr_path: str) -> None:
    prev = cv2.imread(prev_path)
    curr = cv2.imread(curr_path)
    if prev is None:
        raise SystemExit(f"Cannot read {prev_path}")
    if curr is None:
        raise SystemExit(f"Cannot read {curr_path}")

    score = motion_score(prev, curr)
    moved = detect_motion(prev, curr)
    print(f"prev={prev_path}")
    print(f"curr={curr_path}")
    print(f"motion={moved}  score={score:.1f}  threshold={MOTION_THRESHOLD}")


def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    mode = sys.argv[1].lower()
    if mode == "live":
        run_live()
    elif mode == "still" and len(sys.argv) >= 4:
        run_still(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
