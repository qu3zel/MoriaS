cmake_minimum_required(VERSION 3.15)
project(moria VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(moria WIN32
    src/main.cpp
    src/door_art.h
    res/moria.rc
)

target_include_directories(moria PRIVATE src res)
target_compile_definitions(moria PRIVATE UNICODE _UNICODE)
target_link_libraries(moria PRIVATE user32 gdi32 shell32 advapi32)

if(MSVC)
    # Kaynaklar UTF-8 (Türkçe karakterler için /utf-8 şart).
    # Manifest res/moria.rc içinde gömülü; linker'ın kendi manifestini kapat.
    target_compile_options(moria PRIVATE /W4 /utf-8)
    target_link_options(moria PRIVATE /MANIFEST:NO)
    # Statik çalışma zamanı: son kullanıcıdan VC++ Redistributable istemez
    set_property(TARGET moria PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(moria PRIVATE -Wall -Wextra)
    target_link_options(moria PRIVATE -static -static-libgcc -static-libstdc++)
endif()
