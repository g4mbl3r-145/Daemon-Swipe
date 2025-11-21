cmake_minimum_required(VERSION 3.10)
project(TouchpadGestureDaemon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
set(OpenGL_GL_PREFERENCE GLVND)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBINPUT REQUIRED libinput)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(GLFW REQUIRED glfw3)

find_package(OpenGL REQUIRED)

file(GLOB IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/imgui/*.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(gesture_daemon main.cpp ${IMGUI_SOURCES})

target_include_directories(gesture_daemon PRIVATE
    imgui
    imgui/backends
    ${LIBINPUT_INCLUDE_DIRS}
    ${UDEV_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)


target_link_libraries(gesture_daemon PRIVATE
    ${LIBINPUT_LIBRARIES}
    ${UDEV_LIBRARIES}
    ${GLFW_LIBRARIES}
    OpenGL::GL
    dl
    X11
    pthread
)

target_compile_options(gesture_daemon PRIVATE
    ${LIBINPUT_CFLAGS_OTHER}
    ${UDEV_CFLAGS_OTHER}
    ${GLFW_CFLAGS_OTHER}
)

target_compile_definitions(gesture_daemon PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)