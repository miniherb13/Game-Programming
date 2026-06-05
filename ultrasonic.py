"""Ultrasonic distance (TRIG=16, ECHO=18)."""

import time

import RPi.GPIO as GPIO

from config import ECHO_PIN, TRIG_PIN


def get_distance() -> float:
    """Distance in cm. Returns -1 on sensor error."""
    GPIO.output(TRIG_PIN, False)
    time.sleep(0.05)

    GPIO.output(TRIG_PIN, True)
    time.sleep(0.00001)
    GPIO.output(TRIG_PIN, False)

    start = time.time()
    timeout = start + 0.04

    while GPIO.input(ECHO_PIN) == 0:
        start = time.time()
        if time.time() > timeout:
            return -1.0

    stop = time.time()
    timeout = stop + 0.04

    while GPIO.input(ECHO_PIN) == 1:
        stop = time.time()
        if time.time() > timeout:
            return -1.0

    elapsed = stop - start
    return round(elapsed * 34300 / 2, 2)
