cmake_minimum_required(VERSION 3.25)
project(xenon_ie CXX)

add_executable(xenon_ie
    main.cpp
    gfx.cpp
    input.cpp
    store.cpp
    keyboard.cpp
    addressbar.cpp
    hub.cpp
    browser.cpp
)

# local headers root mein hain
target_include_directories(xenon_ie PRIVATE .)

set_target_properties(xenon_ie PROPERTIES
    CXX_STANDARD 23
    CXX_EXTENSIONS ON
)

# -Werror hata do pehle clean build ke liye
target_compile_options(xenon_ie PRIVATE -Wall -Wextra)

xsdk_add_shaders(xenon_ie quad.vert quad.frag)
xsdk_add_xex(xenon_ie TITLE_ID 0x58454945 NAME "Xenon IE")
