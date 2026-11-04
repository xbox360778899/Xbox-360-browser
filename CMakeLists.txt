cmake_minimum_required(VERSION 3.25)
project(xenon_ie CXX)

add_executable(xenon_ie
    src/main.cpp
    src/gfx.cpp
    src/input.cpp
    src/store.cpp
    src/keyboard.cpp
    src/addressbar.cpp
    src/hub.cpp
    src/browser.cpp)
set_target_properties(xenon_ie PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS ON)
target_compile_options(xenon_ie PRIVATE -Wall -Wextra -Werror)

xsdk_add_shaders(xenon_ie shaders/quad.vert shaders/quad.frag)
xsdk_add_xex(xenon_ie TITLE_ID 0x58454945 NAME "Xenon IE")
