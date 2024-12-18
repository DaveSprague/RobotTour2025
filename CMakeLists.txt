# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

set (PICO_SDK_FETCH_FROM_GIT on)
include(pico_sdk_import.cmake)

# define project
project(main C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # LSP

pico_sdk_init()



# target_include_directories(main PRIVATE
#   ${CMAKE_CURRENT_LIST_DIR}/src
#   )

file(GLOB_RECURSE SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/*.c
  ${CMAKE_CURRENT_LIST_DIR}/src/*.h
  ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/BNO08x/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/BNO08x/*.c
  )

add_executable(main
src/main.cpp
${SOURCES})

# generate PIO
pico_generate_pio_header(
  main 
  ${CMAKE_CURRENT_LIST_DIR}/src/quadrature_encoder.pio
  )

  pico_enable_stdio_uart(main 0) # disable UART
  pico_enable_stdio_usb(main 1) # enable USB

pico_add_extra_outputs(main)
target_link_libraries(main 
  pico_stdlib
  pico_multicore
  hardware_pio
  hardware_i2c
  hardware_spi
  hardware_pwm
  )
