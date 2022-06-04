`openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -s tcl`
`cmake -G"NMake Makefiles" ..`
run vscode in developer / admistrator mode for access to nmake (developer command prompt for vs 2019)
check to make sure the "kit" is selected (`GCC for arm-none-eabi 10.2.1`)

see this as an option for installing the toolchain
https://github.com/ndabas/pico-setup-windows

`>C:\Python27\python.exe resources_compiler.py resources/resources.py`

note the forward slashes, it does some kind of replacement on the path to convert to a module name

rates 2x 3/2x 1x 3/4x (triplets) 1/2x 1/4x 1/8x

we want to count up to 0xffffff 8 times per second? at 120bpm

sixteents per second, thirty seconds per second

bpm to bps = 
120 * (1 / 60) * (1/4)

bps to 32nds p sec

bps * 1/8

beats per sample

44100 * 32nds p sec

2,756.25
q8          q4
(44100<<8)/(60<<4) = q4

60*32 = 1920