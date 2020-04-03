# TDM

a pocket sized groovebox.

## changelog
*4/4/2020*
- updates to the backboard schematic

*4/3/2020*
- swapped out the boost converter for a battery charge circuit

*12/30/19*
- screen bitmap font writer (for debug)

## hardware todo
- update layout with all the new stuff
- test the line in cutout
- test the headphone cutout
- get samd11 test board blinking
- check out if stm32f0 is a better solution than samd11
- amp issue might be that I purchased the wrong amp type (a instead of d)
- fix the screen header order (is currently reversed)

## firmware todo

- subtractive synth
- physical modeled drum
- encapsulate the core data
- serialize to flash when changed
- get the current sample thing working with pattern triggers
- convert the screen.ino into a class
- sample to the serial flash
- pitched playback for the sampler
- setup the sleep mode
- setup the gpio sleep interrupt

## later release

- modular synth?
- lua system?