cmake_minimum_required(VERSION 3.16)
project(grafika_projekt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(APP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(APP_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(APP_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_executable(grafika_projekt
    "${APP_SOURCE_DIR}/main.cpp"
    "${APP_SOURCE_DIR}/Box.cpp"
    "${APP_SOURCE_DIR}/Camera.h"
    "${APP_SOURCE_DIR}/Camera.cpp"
    "${APP_SOURCE_DIR}/Scene.h"
    "${APP_SOURCE_DIR}/Scene.cpp"
    "${APP_SOURCE_DIR}/Renderer.h"
    "${APP_SOURCE_DIR}/Renderer.cpp"
    "${APP_SOURCE_DIR}/Model.h"
    "${APP_SOURCE_DIR}/Model.cpp"
    "${APP_SOURCE_DIR}/AnimatedModel.h"
    "${APP_SOURCE_DIR}/AnimatedModel.cpp"
    "${APP_SOURCE_DIR}/Render_Utils.h"
    "${APP_SOURCE_DIR}/Render_Utils.cpp"
    "${APP_SOURCE_DIR}/Shader_Loader.h"
    "${APP_SOURCE_DIR}/Shader_Loader.cpp"
    "${APP_SOURCE_DIR}/objload.h"
    "${APP_SOURCE_DIR}/Mesh.h"            
    "${APP_SOURCE_DIR}/ModelLoader.h"     
    "${APP_SOURCE_DIR}/ModelLoader.cpp"
    "${APP_SOURCE_DIR}/CurvePathGenerator.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(grafika_projekt PRIVATE
    "${APP_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies"
)

find_package(OpenGL REQUIRED)
target_link_libraries(grafika_projekt PRIVATE OpenGL::GL)

if(WIN32)    
    target_include_directories(grafika_projekt PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glew-2.0.0/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw-3.3.8.bin.WIN64/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp/include"
    )    
    target_link_libraries(grafika_projekt PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp/lib/x64/assimp-vc145-mt.lib"
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

    add_custom_command(TARGET grafika_projekt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp/bin/x64/assimp-vc145-mt.dll"
            "$<TARGET_FILE_DIR:grafika_projekt>"
    )

else()    
    find_package(glfw3 REQUIRED)    
    find_package(GLEW REQUIRED)    
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/Assimp/cmake-modules")
    find_package(assimp CONFIG QUIET)
    if(NOT assimp_FOUND)
        find_package(assimp MODULE REQUIRED)
    endif()

    if(TARGET assimp::assimp)
        target_link_libraries(grafika_projekt PRIVATE glfw GLEW::GLEW assimp::assimp)
    else()
        target_include_directories(grafika_projekt PRIVATE ${assimp_INCLUDE_DIRS})
        target_link_libraries(grafika_projekt PRIVATE glfw GLEW::GLEW ${assimp_LIBRARIES})
    endif()
endif()

add_custom_command(TARGET grafika_projekt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${APP_SHADER_DIR}"
        "$<TARGET_FILE_DIR:grafika_projekt>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${APP_ASSET_DIR}"
        "$<TARGET_FILE_DIR:grafika_projekt>/assets"
)
