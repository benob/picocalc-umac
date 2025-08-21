# CMakeLists
#
# MIT License
#
# Copyright (c) 2021, 2024 Matt Evans
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
#
cmake_minimum_required(VERSION 3.13)

# Options that should be defined when initialising the build
# directory with cmake, e.g. "cmake .. -DOPTION=true":
#

set(MEMSIZE 128 CACHE STRING "Memory size, in KB (up to 208 on RP2040, 384 on RP2350 or 4096 with PSRAM)")
option(USE_PSRAM "Use PSRAM (only works with RP2350 chips with PSRAM connected to 0, 8, 19, 47)" OFF)
set(PSRAM_PIN 47 CACHE STRING "Pin for PSRAM (47 for Pimoroni pico plus 2)")

set(DISP_WIDTH 512 CACHE STRING "Display width, can be customized, scrolled if larger than actual display")
set(DISP_HEIGHT 342 CACHE STRING "Display height, can be customized, scrolled if larger than actual display")

set(ROM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/roms/4D1F8172\ -\ MacPlus\ v3.ROM" CACHE STRING "Binary ROM conents, before patching for RAM and display size")

option(USE_SD "Build in SD support, required for reading discs from SD" ON) 
set(DISC0_PATH "${CMAKE_CURRENT_SOURCE_DIR}/discs/system3.3-finder5.5-en.img" CACHE STRING "optional binary disc to be included if SD is not supported") 

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(firmware)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# umac subproject (and Musashi sub-subproject)
set(UMAC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/umac_multidrive)
set(UMAC_MUSASHI_PATH ${UMAC_PATH}/external/Musashi)
set(UMAC_INCLUDE_PATHS ${UMAC_PATH}/include ${UMAC_MUSASHI_PATH})

# This isn't very nice, but hey it's Sunday :p
set(UMAC_SOURCES
  ${UMAC_PATH}/src/disc.c
  ${UMAC_PATH}/src/main.c
  ${UMAC_PATH}/src/rom.c
  ${UMAC_PATH}/src/scc.c
  ${UMAC_PATH}/src/via.c
  ${UMAC_MUSASHI_PATH}/m68kcpu.c
  ${UMAC_MUSASHI_PATH}/m68kdasm.c
  ${UMAC_MUSASHI_PATH}/m68kops.c
  ${UMAC_MUSASHI_PATH}/softfloat/softfloat.c
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DPICO -DMUSASHI_CNF=\\\"../include/m68kconf.h\\\" -DUMAC_MEMSIZE=${MEMSIZE}")

if (USE_PSRAM)
  add_compile_definitions(USE_PSRAM=1)
  add_compile_definitions(PSRAM_PIN=${PSRAM_PIN})
endif()

if (USE_SD)
  add_compile_definitions(USE_SD=1)
  set(FF_DISABLE_RTC ${PICO_RP2350})  # RP2350 doesn't have RTC, so disable it
  add_subdirectory(external/pico_fatfs)
  set(SD_LIBS "pico_fatfs")
else()
  add_custom_command(
    OUTPUT umac-disc.h
    COMMAND xxd -i < "${DISC0_PATH}" > umac-disc.h
    DEPENDS ${DISC0_PATH}
    COMMENT "Converting disc0 to include"
    VERBATIM
  )

  set(NOSD_SOURCES "umac-disc.h")
endif()

add_compile_definitions(DISP_WIDTH=${DISP_WIDTH})
add_compile_definitions(DISP_HEIGHT=${DISP_HEIGHT})

# Patch ROM and generate umac-rom.h for inlining contents
add_custom_command(
  OUTPUT umac-rom.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gen-rom.sh ${ROM_PATH} "umac-rom.h" ${MEMSIZE} ${DISP_WIDTH} ${DISP_HEIGHT}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen-rom.sh ${ROM_PATH}
  COMMENT "Patching ROM for memory and display size, manually exit umac with <ctrl-Q> or <command-Q>"
  VERBATIM
)

add_executable(firmware
  src/main.c
  src/video.c
  src/kbd.c
  src/hid.c

  src/lcd_3bit.c
  src/keyboard.c

  umac-rom.h
  ${NOSD_SOURCES}
  ${UMAC_SOURCES}
  )

# The umac sources need to prepare Musashi (some sources are generated):
add_custom_command(OUTPUT ${UMAC_MUSASHI_PATH}/m68kops.c
  COMMAND echo "*** Preparing umac source ***"
  COMMAND make -C ${UMAC_PATH} prepare
  )
add_custom_target(prepare_umac
  DEPENDS ${UMAC_MUSASHI_PATH}/m68kops.c
  )
#add_dependencies(firmware prepare_umac)

target_link_libraries(firmware
  pico_stdlib
  pico_multicore
  hardware_dma
  hardware_pio
  hardware_sync
  hardware_spi
  hardware_i2c
  ${SD_LIBS}
  )

target_include_directories(firmware PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${UMAC_INCLUDE_PATHS}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

pico_enable_stdio_usb(firmware 0)
pico_enable_stdio_uart(firmware 1)

# Needed for UF2:
pico_add_extra_outputs(firmware)


