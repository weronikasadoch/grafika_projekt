cmake_minimum_required(VERSION 3.16)

project(grafika_projekt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(APP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(APP_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

add_executable(grafika_projekt
    "${APP_SOURCE_DIR}/main.cpp"
    "${APP_SOURCE_DIR}/Box.cpp"
    "${APP_SOURCE_DIR}/Camera.h"
    "${APP_SOURCE_DIR}/Camera.cpp"
    "${APP_SOURCE_DIR}/Scene.h"
    "${APP_SOURCE_DIR}/Scene.cpp"
    "${APP_SOURCE_DIR}/Renderer.h"
    "${APP_SOURCE_DIR}/Renderer.cpp"
    "${APP_SOURCE_DIR}/Render_Utils.h"
    "${APP_SOURCE_DIR}/Render_Utils.cpp"
    "${APP_SOURCE_DIR}/Shader_Loader.h"
    "${APP_SOURCE_DIR}/Shader_Loader.cpp"
    "${APP_SOURCE_DIR}/objload.h"
)

target_include_directories(grafika_projekt PRIVATE
    "${APP_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm"
)

find_package(OpenGL REQUIRED)
target_link_libraries(grafika_projekt PRIVATE OpenGL::GL)

if(WIN32)
    target_include_directories(grafika_projekt PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glew-2.0.0/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw-3.3.8.bin.WIN64/include"
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(GLFW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw-3.3.8.bin.WIN64")
        set(GLEW_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glew-2.0.0/lib/Release/x64")
    else()
        message(FATAL_ERROR "This cleaned project keeps only 64-bit Windows libraries. Configure CMake for x64.")
    endif()

    target_compile_definitions(grafika_projekt PRIVATE GLEW_STATIC)

    if(MSVC)
        if(EXISTS "${GLFW_ROOT}/lib-vc2022/glfw3.lib")
            set(GLFW_LIB_DIR "${GLFW_ROOT}/lib-vc2022")
        else()
            set(GLFW_LIB_DIR "${GLFW_ROOT}/lib-vc2019")
        endif()
        target_link_libraries(grafika_projekt PRIVATE
            "${GLEW_LIB_DIR}/glew32s.lib"
            "${GLFW_LIB_DIR}/glfw3.lib"
            user32
            gdi32
            shell32
        )
    else()
        target_link_libraries(grafika_projekt PRIVATE
            "${GLEW_LIB_DIR}/glew32s.lib"
            "${GLFW_ROOT}/lib-mingw-w64/libglfw3.a"
            gdi32
            shell32
            user32
        )
    endif()
else()
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)

    target_link_libraries(grafika_projekt PRIVATE glfw GLEW::GLEW)
endif()

add_custom_command(TARGET grafika_projekt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${APP_SHADER_DIR}"
        "$<TARGET_FILE_DIR:grafika_projekt>/shaders"
)
