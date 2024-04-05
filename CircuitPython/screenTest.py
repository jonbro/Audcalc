import board
import displayio
import terminalio
import adafruit_displayio_ssd1306
from adafruit_display_text import label
import busio
import time
import analogio

displayio.release_displays()

oled_reset = board.GP21

# Use for I2C
i2c = busio.I2C(scl=board.GP3, sda=board.GP2)
display_bus = displayio.I2CDisplay(i2c, device_address=0x3C, reset=oled_reset)

WIDTH = 128
HEIGHT = 32
BORDER = 5

display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=WIDTH, height=HEIGHT, rotation=180)
batteryLevel = analogio.AnalogIn(board.GP28)


# Make the display context
splash = displayio.Group()
display.show(splash)

text_group = displayio.Group(scale=1, x=0, y=8)
text = "Hello World!"
text_area = label.Label(terminalio.FONT, text=text, color=0xFFFF00)
text_group.append(text_area)  # Subgroup for text scaling
splash.append(text_group)

def to_voltage(val):
    return ((val * 3.3) / 65536)*1.75/.75

while True:
    text_area.text = "power level:\n{}".format(to_voltage(batteryLevel.value))
    time.sleep(0.01)
    
batteryLevel.deinit()
