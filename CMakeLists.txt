cmake_minimum_required(VERSION 3.12)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(vermin_vm LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REUIRED True)

include_directories(./include)

if( MSVC )
    SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup" )
endif()

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SOURCES
     "./include/*.h"
     "./source/*.c"
)

add_executable(vermin ${SOURCES})
