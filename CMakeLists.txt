cmake_minimum_required(VERSION 3.25)
project(xenon_ie CXX)

# Saari source files yahan list karo (Root folder ke hisaab se)
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

# Headers root mein hain, isliye include path "." (current directory) hai
target_include_directories(xenon_ie PRIVATE .)

# C++23 standard set karo (xsdk GCC 15.3 isko support karta hai)
set_target_properties(xenon_ie PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)

# Warnings ko enable karo, par -Werror hata do taaki build break na ho
target_compile_options(xenon_ie PRIVATE -Wall -Wextra)

# Shaders add karo (Yeh root mein hain)
xsdk_add_shaders(xenon_ie quad.vert quad.frag)

# Final .xex executable generate karne ke liye
xsdk_add_xex(xenon_ie TITLE_ID 0x58454945 NAME "Xenon IE")
