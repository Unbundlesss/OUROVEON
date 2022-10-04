
#include <GLFW/glfw3.h>
#include <GLFW/glfw3ext.h>

#define _MIN(a,b) ((a)<(b) ? (a):(b))
#define _MAX(a,b) ((a)>(b) ? (a):(b))

int glfwSetWindowCenter( GLFWwindow* window )
{
    if ( !window )
        return 0;

    int sx = 0, sy = 0;
    int px = 0, py = 0;
    int mx = 0, my = 0;
    int monitor_count = 0;
    int best_area = 0;
    int final_x = 0, final_y = 0;

    glfwGetWindowSize( window, &sx, &sy );
    glfwGetWindowPos( window, &px, &py );

    // Iterate through all monitors
    GLFWmonitor** m = glfwGetMonitors( &monitor_count );
    if ( !m )
        return 0;

    for ( int j = 0; j < monitor_count; ++j )
    {
        glfwGetMonitorPos( m[j], &mx, &my );
        const GLFWvidmode* mode = glfwGetVideoMode( m[j] );
        if ( !mode )
            continue;

        // Get intersection of two rectangles - screen and window
        int minX = _MAX( mx, px );
        int minY = _MAX( my, py );

        int maxX = _MIN( mx + mode->width, px + sx );
        int maxY = _MIN( my + mode->height, py + sy );

        // Calculate area of the intersection
        int area = _MAX( maxX - minX, 0 ) * _MAX( maxY - minY, 0 );

        // If its bigger than actual (window covers more space on this monitor)
        if ( area > best_area )
        {
            // Calculate proper position in this monitor
            final_x = mx + (mode->width - sx) / 2;
            final_y = my + (mode->height - sy) / 2;

            best_area = area;
        }

    }

    // We found something
    if ( best_area )
        glfwSetWindowPos( window, final_x, final_y );

    // Something is wrong - current window has NOT any intersection with any monitors. Move it to the default one.
    else
    {
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        if ( primary )
        {
            const GLFWvidmode* desktop = glfwGetVideoMode( primary );

            if ( desktop )
                glfwSetWindowPos( window, (desktop->width - sx) / 2, (desktop->height - sy) / 2 );
            else
                return 0;
        }
        else
            return 0;
    }

    return 1;
}

#undef _MIN
#undef _MAX

int glfwIsWindowPositionValid( GLFWwindow* window, int windowX, int windowY )
{
    if ( !window )
        return 0;

    int monitor_count = 0;
    GLFWmonitor** m = glfwGetMonitors( &monitor_count );
    if ( !m )
        return 0;

    int mx = 0, my = 0;
    int validCount = monitor_count;

    for ( int j = 0; j < monitor_count; ++j )
    {
        glfwGetMonitorPos( m[j], &mx, &my );
        const GLFWvidmode* mode = glfwGetVideoMode( m[j] );
        if ( !mode )
            continue;

        if ( windowX < mx ||
             windowY < my ||
             windowX >= ( mx + mode->width ) ||
             windowY >= ( my + mode->height )
            )
        {
            validCount--;
        }
    }

    return validCount;
}
