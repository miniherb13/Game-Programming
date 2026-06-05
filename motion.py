"""Camera motion detection."""

import cv2

from config import MOTION_THRESHOLD


def _roi_gray(bgr):
    h, w = bgr.shape[:2]
    roi = bgr[h // 4 : 3 * h // 4, w // 4 : 3 * w // 4]
    return cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)


def motion_score(prev_bgr, curr_bgr) -> float:
    """ROI diff mean between two frames (for tuning)."""
    if prev_bgr is None or curr_bgr is None:
        return 0.0
    prev_gray = _roi_gray(prev_bgr)
    curr_gray = _roi_gray(curr_bgr)
    diff = cv2.absdiff(prev_gray, curr_gray)
    return float(diff.mean())


def detect_motion(prev_bgr, curr_bgr, threshold=None) -> bool:
    """True if center ROI diff mean exceeds threshold."""
    th = MOTION_THRESHOLD if threshold is None else threshold
    return motion_score(prev_bgr, curr_bgr) > th
