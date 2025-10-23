#include "OpenGLUtils.h"

using namespace juce::gl;

namespace OpenGLUtils
{
    // Windows-specific OpenGL initialization
    void initializePlatformSpecific()
    {
        // Windows doesn't need special initialization
        // DirectX/OpenGL interop is handled by JUCE
    }
    
    // Error checking utility
    bool checkOpenGLError(const char* operation)
    {
        GLenum error = juce::gl::glGetError();
        if (error != juce::gl::GL_NO_ERROR)
        {
            const char* errorString = nullptr;
            switch (error)
            {
                case juce::gl::GL_INVALID_ENUM:      errorString = "GL_INVALID_ENUM"; break;
                case juce::gl::GL_INVALID_VALUE:     errorString = "GL_INVALID_VALUE"; break;
                case juce::gl::GL_INVALID_OPERATION: errorString = "GL_INVALID_OPERATION"; break;
                case juce::gl::GL_OUT_OF_MEMORY:     errorString = "GL_OUT_OF_MEMORY"; break;
                default:                              errorString = "Unknown GL error"; break;
            }
            
            DBG("OpenGL error during " << operation << ": " << errorString);
            return false;
        }
        return true;
    }
    
    // Get OpenGL renderer information
    juce::String getOpenGLRendererInfo()
    {
        juce::String info;
        
        const GLubyte* vendor = juce::gl::glGetString(juce::gl::GL_VENDOR);
        const GLubyte* renderer = juce::gl::glGetString(juce::gl::GL_RENDERER);
        const GLubyte* version = juce::gl::glGetString(juce::gl::GL_VERSION);
        const GLubyte* glslVersion = juce::gl::glGetString(juce::gl::GL_SHADING_LANGUAGE_VERSION);
        
        if (vendor)      info << "Vendor: " << (const char*)vendor << "\n";
        if (renderer)    info << "Renderer: " << (const char*)renderer << "\n";
        if (version)     info << "OpenGL Version: " << (const char*)version << "\n";
        if (glslVersion) info << "GLSL Version: " << (const char*)glslVersion << "\n";
        
        return info;
    }
}