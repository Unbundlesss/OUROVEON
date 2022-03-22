//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

namespace gl {

inline const char* getGlErrorText( GLenum err )
{
    switch ( err )
    {
    case GL_NO_ERROR:                       return "GL_NO_ERROR";
    case GL_INVALID_ENUM:                   return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                  return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:              return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY";
    }
    return "Unknown Error";
}

} // namespace gl

#define glChecked( ... )    \
        __VA_ARGS__;        \
        { \
            const auto glErr = glGetError(); \
            if ( glErr != GL_NO_ERROR ) \
            { \
                blog::error::app( "{}:{} OpenGL call error [{}] : {}", __FUNCTION__, __LINE__, glErr, gl::getGlErrorText(glErr) ); \
                blog::error::app( "{}", #__VA_ARGS__ ); \
            } \
        }

