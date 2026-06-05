"""카메라 움직임 감지 — A 담당"""

import cv2
import numpy as np

from config import MOTION_THRESHOLD


def detect_motion(prev_bgr, curr_bgr, threshold=None) -> bool:
    """ROI 중앙 영역의 grayscale diff mean이 threshold 초과이면 True."""
    if prev_bgr is None or curr_bgr is None:
        return False

    th = MOTION_THRESHOLD if threshold is None else threshold

    def roi_gray(bgr):
        h, w = bgr.shape[:2]
        roi = bgr[h // 4 : 3 * h // 4, w // 4 : 3 * w // 4]
        return cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)

    prev_gray = roi_gray(prev_bgr)
    curr_gray = roi_gray(curr_bgr)
    diff = cv2.absdiff(prev_gray, curr_gray)
    return float(diff.mean()) > th
