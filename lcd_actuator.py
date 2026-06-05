"""LCD, buzzer, LED, ultrasonic output."""

import time

import RPi.GPIO as GPIO

from config import BUZZER_PIN, ECHO_PIN, LCD_ADDR, LCD_ENABLED, LED_PIN, TRIG_PIN
from ultrasonic import get_distance

if LCD_ENABLED:
    from RPLCD.i2c import CharLCD


class LCD:
    def __init__(self, addr=0x27):
        self.lcd = CharLCD(
            i2c_expander="PCF8574", address=addr, port=1, cols=16, rows=2
        )
        self.lcd.clear()

    def show(self, line1="", line2=""):
        self.lcd.cursor_pos = (0, 0)
        self.lcd.write_string(line1[:16].ljust(16))
        self.lcd.cursor_pos = (1, 0)
        self.lcd.write_string(line2[:16].ljust(16))

    def close(self):
        self.lcd.clear()
        self.lcd.close()


class Actuator:
    def __init__(self):
        GPIO.setwarnings(False)
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(TRIG_PIN, GPIO.OUT)
        GPIO.setup(ECHO_PIN, GPIO.IN)
        GPIO.setup(LED_PIN, GPIO.OUT)
        GPIO.setup(BUZZER_PIN, GPIO.OUT)

        GPIO.output(LED_PIN, GPIO.LOW)
        GPIO.output(BUZZER_PIN, GPIO.LOW)

        self.lcd = LCD(LCD_ADDR) if LCD_ENABLED else None

    def read_distance(self) -> float:
        return get_distance()

    def _lcd(self, line1, line2=""):
        print(f"[LCD] {line1} | {line2}")
        if self.lcd:
            self.lcd.show(line1, line2)

    def _led(self, on: bool):
        GPIO.output(LED_PIN, GPIO.HIGH if on else GPIO.LOW)

    def _buzzer_beep(self, seconds=0.4):
        GPIO.output(BUZZER_PIN, GPIO.HIGH)
        time.sleep(seconds)
        GPIO.output(BUZZER_PIN, GPIO.LOW)

    def show_idle(self, distance=None):
        self._led(False)
        GPIO.output(BUZZER_PIN, GPIO.LOW)
        if distance is not None and distance >= 0:
            self._lcd("Companion Bot", f"{distance} cm")
        else:
            self._lcd("Companion Bot", "Zzz...")

    def show_motion(self, distance=None):
        self._led(True)
        line2 = f"{distance} cm" if distance is not None and distance >= 0 else "I see you!"
        self._lcd("Hi there!", line2[:16])
        self._buzzer_beep(0.4)

    def show_reply(self, line1, line2=""):
        self._led(True)
        self._lcd(line1, line2)

    def close(self):
        GPIO.output(BUZZER_PIN, GPIO.LOW)
        GPIO.output(LED_PIN, GPIO.LOW)
        if self.lcd:
            self.lcd.close()
        GPIO.cleanup((TRIG_PIN, ECHO_PIN, LED_PIN, BUZZER_PIN))


if __name__ == "__main__":
    act = Actuator()
    try:
        act.show_idle()
        time.sleep(2)
        d = act.read_distance()
        print(f"distance={d} cm")
        act.show_motion(d)
        time.sleep(2)
        act.show_reply("Hello!", "How are you?")
        time.sleep(2)
    finally:
        act.close()
