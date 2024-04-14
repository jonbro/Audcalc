# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/nanopb/extra)

add_definitions(-DLUA_USE_C89)
set(luaSrcDir "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.3/src")
set(luaSrcFiles lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c ldo.c
	ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c
	loslib.c lparser.c lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c
	lutf8lib.c lvm.c lzio.c)
list(TRANSFORM luaSrcFiles PREPEND ${luaSrcDir}/)
set(LUA_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.3/src")
include_directories(${LUA_INCLUDE_DIRS})

find_package(Nanopb REQUIRED)
include_directories(${NANOPB_INCLUDE_DIRS})
nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS
        VoiceDataInternal.proto
        ParamLockPoolInternal.proto
        SongDataInternal.proto
        GlobalData.proto
)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "C:/Users/jonbro/Documents/Pico/pico-sdk")
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(PICO_BOARD "tdm_board")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(tdm C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(tdm
        main.cc
        multicore_support.c
        # usb_audio.c
        usb_descriptors.c
        diagnostics.cc
        GrooveBox.cc
        GrooveBoxTestUtils.cc
        voice_data.cc
        SongData.cc
        ParamLockPool.cpp
        Instrument.cc
        Midi.cc
        MidiParamMapper.cc
        tlv320driver.c
        ssd1306.c
        ws2812.c
        hardware.c
        Serializer.cc
        # USBSerialDevice.cc
        filesystem.c
        audio/macro_oscillator.cc
        i2c_dma.c
        audio/analog_oscillator.cc
        audio/digital_oscillator.cc
        audio/resources.cc
        audio/random.cc 
        AudioSampleSecondbloop.cpp
        m6x118pt7b.cc
        ${luaSrcFiles}
        ${PROTO_SRCS} ${PROTO_HDRS}
)
pico_generate_pio_header(tdm ${CMAKE_CURRENT_LIST_DIR}/input_output_copy_i2s.pio)
pico_generate_pio_header(tdm ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_set_program_name(tdm "tdm")
pico_set_program_version(tdm "0.1")

pico_enable_stdio_usb(tdm 0)
pico_enable_stdio_uart(tdm 1)


target_include_directories(tdm PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Add any user requested libraries
target_link_libraries(tdm
        pico_stdlib
        pico_multicore
        hardware_sleep
        hardware_i2c
        hardware_pio
        hardware_adc
        hardware_flash
        pico_rand
        pico_audio_i2s
        tinyusb_device tinyusb_board
        )
target_compile_definitions(tdm PRIVATE
# PICO_AUDIO_I2S_MONO_INPUT=1
# #define for our example code
# USE_AUDIO_I2S=1
)

pico_add_extra_outputs(tdm) 