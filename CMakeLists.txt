cmake_minimum_required(VERSION 3.15)
project(MustyClient VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(MustyClient SHARED
    src/main.cpp
    src/core/JNIHelper.cpp
    src/core/JNIHelper.h
    src/modules/ModuleManager.cpp
    src/modules/ModuleManager.h
    src/modules/Module.h
    src/modules/combat/Killaura.cpp
    src/modules/combat/Killaura.h
    src/modules/combat/Aimbot.cpp
    src/modules/combat/Aimbot.h
    src/modules/render/ESP.cpp
    src/modules/render/ESP.h
    src/modules/render/XRay.cpp
    src/modules/render/XRay.h
)

find_package(JNI REQUIRED)
target_include_directories(MustyClient PRIVATE ${JNI_INCLUDE_DIRS} src)
target_link_libraries(MustyClient PRIVATE ${JNI_LIBRARIES})

if(WIN32)
    target_link_libraries(MustyClient PRIVATE user32 gdi32 gdiplus)
    set_target_properties(MustyClient PROPERTIES PREFIX "")
endif()
