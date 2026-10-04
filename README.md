# AudCalc

![a large shot of audcalc in front of an orange background.](https://audcalc.jonathanbrodsky.com/images/hero_image.png)

AudCalc is a portable synthesizer and sequencer.

It has 8 voices which can switch between synthesis (using Mutable Instruments Braids cores) and sampling. The sequencer supports parameter locks, conditional triggers, and various time signatures and loop lengths. There are also two built in effects sends for building a full track. It's fun to jam with by itself, and can link into your larger portable music setup via midi or cv clock.

[Website](https://audcalc.jonathanbrodsky.com) | [Manual](https://audcalc.jonathanbrodsky.com/docs.html) | [Shop](https://thegoblingroup.com/products/audcalc) | [Contact](https://audcalc.jonathanbrodsky.com/contact.html)

## Specifications

### Synthesizer

- Many different oscillators from MI Braids
- 2 assignable AD envelopes
- Lowpass filter
- Volume, pan, legato, finetune
- 2 effects sends (delay & reverb)
- Assignable LFO
- Simple sampler (sampling via line in or onboard mic)
- 16 patches, 8 simultaneous monophonic voices

### Sequencer

- 16 patterns, with assignable lengths from 1-64 steps
- 4096 pattern lock locations for overriding synthesizer settings per step
- Each of the 16 patches can run at 2x-1/8 bpm rate, with some fun subdivisions
- Conditional triggers
- Pattern chaining
- Step ratcheting / retriggers

### Hardware

- RP2040 core
- 1000mAh rechargeable and replaceable battery, USB-C charging
- TRS midi in/out, audio in/out (3.5mm TRS), USB midi
- 100x65x25mm - approximately the size of a cassette tape box + knob height
- Body made with FDM black PLA, buttons with FDM translucent TPU
- FR4 (circuitboard) faceplate
- OLED display

## Building one

The BOM is contained within the Hardware KiCad project, ready for assembly via JLCPCB.
There are several additional items that must be sourced and assembled:

| part                         | count | mfg part number | link                                                  | notes                                                                                 |
|------------------------------|-------|-----------------|-------------------------------------------------------|---------------------------------------------------------------------------------------|
| oled screen (128x32)         | 1     | ssd1306         | https://www.aliexpress.com/item/4000842671330.html    | I'm using the white / solderable version.                                             |
| battery (50x34x5mm, ~1000mAh) | 1    | 503450          | search for "503450" with a JST-PH connector           | Widely available from many vendors. Confirm the correct polarity before using.        |
| m2 4mm f-f standoff          | 4     |                 | https://www.aliexpress.com/item/1005002145042844.html |                                                                                       |
| m2 6mm m-f standoff          | 4     |                 | https://www.aliexpress.com/item/1005002145042844.html |                                                                                       |
| m2 3mm hex head bolt (black) | 8     |                 | https://www.aliexpress.com/item/10000148429238.html   |                                                                                       |
