#pragma once

#include <juce_opengl/juce_opengl.h>

namespace OpenGLUtils
{
    // OpenGL version requirements for Windows
    inline juce::OpenGLContext::OpenGLVersion getRecommendedOpenGLVersion()
    {
        return juce::OpenGLContext::OpenGLVersion::openGL3_2;
    }
    
    // Configure OpenGL context for best performance on Windows
    inline void configureOpenGLContext(juce::OpenGLContext& context)
    {
        // Set pixel format for multisampling
        juce::OpenGLPixelFormat pixelFormat;
        pixelFormat.multisamplingLevel = 4; // 4x MSAA
        pixelFormat.redBits = 8;
        pixelFormat.greenBits = 8;
        pixelFormat.blueBits = 8;
        pixelFormat.alphaBits = 8;
        pixelFormat.depthBufferBits = 24;
        pixelFormat.stencilBufferBits = 8;
        
        context.setPixelFormat(pixelFormat);
        context.setOpenGLVersionRequired(getRecommendedOpenGLVersion());
        
        // Enable V-Sync for smooth rendering
        context.setSwapInterval(1);
        
        // Set continuous repainting for animations
        context.setContinuousRepainting(false); // Set to true only when animating
    }
    
    // Check if OpenGL is available and properly initialized
    inline bool isOpenGLAvailable(juce::OpenGLContext& context)
    {
        return context.isAttached() && context.isActive();
    }
    
    // Windows-specific OpenGL setup
    void initializePlatformSpecific();
    
    // Error checking utility
    bool checkOpenGLError(const char* operation);
    
    // Get OpenGL renderer information
    juce::String getOpenGLRendererInfo();
    
    // Simple shader source for basic rendering
    namespace Shaders
    {
        const char* const vertexShader = R"(
            #version 330 core
            layout(location = 0) in vec3 position;
            layout(location = 1) in vec4 colour;
            
            out vec4 vertexColour;
            
            uniform mat4 projectionMatrix;
            uniform mat4 viewMatrix;
            
            void main()
            {
                gl_Position = projectionMatrix * viewMatrix * vec4(position, 1.0);
                vertexColour = colour;
            }
        )";
        
        const char* const fragmentShader = R"(
            #version 330 core
            in vec4 vertexColour;
            out vec4 fragmentColour;
            
            void main()
            {
                fragmentColour = vertexColour;
            }
        )";
    }
    
    // Helper class for OpenGL rendering
    class OpenGLHelper
    {
    public:
        OpenGLHelper() = default;
        ~OpenGLHelper() = default;
        
        void initialise(juce::OpenGLContext& context)
        {
            createShaders(context);
        }
        
        void shutdown()
        {
            shader.reset();
        }
        
        void render(juce::OpenGLContext& context)
        {
            juce::OpenGLHelpers::clear(juce::Colour(0xff1a1a1a));
        }
        
    private:
        std::unique_ptr<juce::OpenGLShaderProgram> shader;
        
        void createShaders(juce::OpenGLContext& context)
        {
            shader = std::make_unique<juce::OpenGLShaderProgram>(context);
            
            if (shader->addVertexShader(Shaders::vertexShader) &&
                shader->addFragmentShader(Shaders::fragmentShader) &&
                shader->link())
            {
                shader->use();
            }
            else
            {
                shader.reset();
            }
        }
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLHelper)
    };
}