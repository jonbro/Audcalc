# SPDX-FileCopyrightText: 2021 Kattni Rembor for Adafruit Industries
#
# SPDX-License-Identifier: MIT

"""Example for Pico. Turns on the built-in LED."""
import board
import digitalio
import time

led = digitalio.DigitalInOut(board.GP25)
led.direction = digitalio.Direction.OUTPUT

class LedBlinker:
    BLINK_ON_DURATION = 0.5
    # How long we want the LED to stay off
    BLINK_OFF_DURATION = 0.25
    # When we last changed the LED state
    LAST_BLINK_TIME = -1
    # Store the current time to refer to later.
    def blink(self):
        now = time.monotonic()
        if not led.value:
            # Is it time to turn on?
            if now >= self.LAST_BLINK_TIME + self.BLINK_OFF_DURATION:
                led.value = True
                self.LAST_BLINK_TIME = now
        if led.value:
            # Is it time to turn off?
            if now >= self.LAST_BLINK_TIME + self.BLINK_ON_DURATION:
                led.value = False
                self.LAST_BLINK_TIME = now

blinker = LedBlinker()
while True:
    blinker.blink()