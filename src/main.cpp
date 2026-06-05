#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "Scene.h"

#include <iostream>

namespace
{
    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 720;

    void framebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        auto* scene = static_cast<Scene*>(glfwGetWindowUserPointer(window));
        if (scene != nullptr)
        {
            scene->updateFramebufferSize(width, height);
        }

        glViewport(0, 0, width, height);
    }

    void updateFramebufferSize(GLFWwindow* window)
    {
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        framebufferSizeCallback(window, framebufferWidth, framebufferHeight);
    }
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        kWindowWidth,
        kWindowHeight,
        "Cartoon Underwater World",
        nullptr,
        nullptr
    );

    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    Scene scene(kWindowWidth, kWindowHeight);
    glfwSetWindowUserPointer(window, &scene);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    updateFramebufferSize(window);

    Renderer renderer;
    if (!renderer.initialize())
    {
        std::cerr << "Failed to initialize renderer\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    scene.updateDeltaTime(static_cast<float>(glfwGetTime()));

    while (!glfwWindowShouldClose(window))
    {
        scene.updateDeltaTime(static_cast<float>(glfwGetTime()));
        scene.processInput(window);
        renderer.render(scene);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
