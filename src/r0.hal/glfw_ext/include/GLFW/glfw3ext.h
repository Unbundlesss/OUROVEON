#pragma once


#ifdef __cplusplus
extern "C" {
#endif

// https://github.com/glfw/glfw/issues/310
extern int glfwSetWindowCenter( GLFWwindow* window );

// check monitor layout to ensure we're not about to place a window offscreen (eg. if someone's rejiggled their monitor setup between runs)
extern int glfwIsWindowPositionValid( GLFWwindow* window, int windowX, int windowY );

#ifdef __cplusplus
}
#endif