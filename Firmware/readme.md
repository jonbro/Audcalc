`openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -s tcl`
`cmake -G"NMake Makefiles" ..`
run vscode in developer / admistrator mode for access to nmake (developer command prompt for vs 2019)
check to make sure the "kit" is selected (`GCC for arm-none-eabi 10.2.1`)

see this as an option for installing the toolchain
https://github.com/ndabas/pico-setup-windows

`>C:\Python27\python.exe resources_compiler.py resources/resources.py`

note the forward slashes, it does some kind of replacement on the path to convert to a module name