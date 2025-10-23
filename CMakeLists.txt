cmake_minimum_required(VERSION 3.22)
project(DrumGroovePro VERSION 0.9.9)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE - using relative path as requested (no hardcoded paths)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE juce_build)

# Add the plugin target - NOW WITH STANDALONE
juce_add_plugin(DrumGroovePro
    PRODUCT_NAME "DrumGroovePro"
    COMPANY_NAME "InToEtherion"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE InTE
    PLUGIN_CODE Dgp1
    FORMATS VST3 Standalone  # ADDED STANDALONE HERE
    VST3_CATEGORIES "Fx" "Tools" "MIDI"
)

# Generate JUCE header
juce_generate_juce_header(DrumGroovePro)

# Add source files - Complete list with proper structure
target_sources(DrumGroovePro PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GUI/MainComponent.cpp
    Source/GUI/Components/HeaderSection.cpp
    Source/GUI/Components/GrooveBrowser.cpp
    Source/GUI/Components/DrumPartsColumn.cpp
    Source/GUI/Components/Track.cpp
    Source/GUI/Components/MultiTrackContainer.cpp
    Source/GUI/Components/TimelineControls.cpp
    Source/GUI/Components/TrackHeader.cpp
    Source/GUI/Components/FolderPanel.cpp
    Source/GUI/Components/AboutDialog.cpp
    Source/GUI/Components/AddFolderDialog.cpp
    Source/GUI/Components/CustomFileBrowser.cpp
    Source/GUI/Components/FilePathDisplay.cpp
    Source/GUI/Components/TimelineManager.cpp
    Source/GUI/LookAndFeel/DrumGrooveLookAndFeel.cpp
    Source/Core/MidiProcessor.cpp
    Source/Core/MidiDissector.cpp
    Source/Core/DrumLibraryManager.cpp
    Source/Core/FavoritesManager.cpp
    Source/Utils/OpenGLUtils.cpp
)

# Include directories
target_include_directories(DrumGroovePro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI/Components
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI/LookAndFeel
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Utils
)

# Compile definitions - Modern JUCE 8+ settings
target_compile_definitions(DrumGroovePro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_OPENGL=1
    JUCE_ENABLE_REPAINT_DEBUGGING=0
    JUCE_USE_DIRECTWRITE=1
    JUCE_ENABLE_LIVE_CONSTANT_EDITOR=0
)

# Debug/Release specific definitions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(DrumGroovePro PUBLIC
        DEBUG=1
        _DEBUG=1
        JUCE_DEBUG=1
        JUCE_CHECK_MEMORY_LEAKS=1
    )
else()
    target_compile_definitions(DrumGroovePro PUBLIC
        NDEBUG=1
        JUCE_DEBUG=0
        JUCE_CHECK_MEMORY_LEAKS=0
    )
endif()

# Windows-specific settings for Windows 11
if(WIN32)
    target_compile_definitions(DrumGroovePro PUBLIC
        NOMINMAX
        UNICODE
        _UNICODE
        WINVER=0x0A00          # Windows 10/11
        _WIN32_WINNT=0x0A00    # Windows 10/11
        WIN32_LEAN_AND_MEAN
    )
    
    # Windows compiler flags for MSVC (Visual Studio 2022)
    if(MSVC)
        target_compile_options(DrumGroovePro PRIVATE
            /W4                 # Warning level 4
            /permissive-        # Conformance mode for C++20
            /Zc:preprocessor    # Use conforming preprocessor
            /EHsc               # Enable C++ exceptions
            /MP                 # Multi-processor compilation
            /fp:fast            # Fast floating point
            /utf-8              # Set source and execution character sets to UTF-8
            /Zc:__cplusplus     # Enable correct __cplusplus macro
            
            # Disable specific warnings for JUCE 8+ compatibility
            /wd4458             # Declaration hides class member
            /wd4456             # Declaration hides previous local declaration
            /wd4457             # Declaration hides function parameter
            /wd4251             # DLL interface warnings
            /wd4996             # Deprecated function warnings
        )
        
        # Release optimizations for performance
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(DrumGroovePro PRIVATE
                /O2             # Maximum optimization
                /Ob2            # Inline function expansion
                /Oi             # Enable intrinsic functions
                /Ot             # Favor fast code
                /GL             # Whole program optimization
                /arch:AVX2      # Enable AVX2 for hardware acceleration
                /Gw             # Optimize global data
                /Gy             # Enable function-level linking
            )
            target_link_options(DrumGroovePro PRIVATE
                /LTCG           # Link-time code generation
                /OPT:REF        # Eliminate unreferenced code
                /OPT:ICF        # Enable COMDAT folding
                /INCREMENTAL:NO # Disable incremental linking for release
            )
        endif()
    endif()
endif()

# Link JUCE modules - Complete list for plugin functionality
target_link_libraries(DrumGroovePro PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Windows OpenGL libraries for hardware acceleration
if(WIN32)
    target_link_libraries(DrumGroovePro PRIVATE
        opengl32
        glu32
        d3d11      # DirectX for hardware acceleration
        dxgi       # DirectX Graphics Infrastructure
    )
endif()

# Set output directories
set_target_properties(DrumGroovePro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Resource copying - Build directory first
add_custom_command(TARGET DrumGroovePro_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying resources to build VST3 bundle..."
    
    # Create directory structure in build folder
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/icons/folder/16x16"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/icons/midi/16x16"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/logo"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/background/"
    
    # Copy icon files to build folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/folder/16x16/folder.png"
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/icons/folder/16x16/folder.png"
        || ${CMAKE_COMMAND} -E echo "Warning: folder.png not found"
        
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/midi/16x16/midi.png"
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/icons/midi/16x16/midi.png"
        || ${CMAKE_COMMAND} -E echo "Warning: midi.png not found"
    
    # Copy logo to build folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/logo/logo.png"
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/logo/logo.png"
        || ${CMAKE_COMMAND} -E echo "Warning: logo.png not found"

    # Copy background to build folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/background/background.png"
        "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources/background/background.png"
        || ${CMAKE_COMMAND} -E echo "Warning: logo.png not found"
    
    COMMENT "Resources copied to build directory"
    VERBATIM
)

# Final system installation with error handling
add_custom_command(TARGET DrumGroovePro_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying resources to system VST3 folder..."
    
    # Wait for JUCE installation to complete
    COMMAND ${CMAKE_COMMAND} -E sleep 1
    
    # Create directories with proper error handling
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/icons/folder/16x16"
        || ${CMAKE_COMMAND} -E echo "Note: Could not create system folders (may need admin rights)"
        
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/icons/midi/16x16"
        || ${CMAKE_COMMAND} -E echo "Note: Could not create system folders (may need admin rights)"
        
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/logo"
        || ${CMAKE_COMMAND} -E echo "Note: Could not create system folders (may need admin rights)"

    COMMAND ${CMAKE_COMMAND} -E make_directory
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/background"
        || ${CMAKE_COMMAND} -E echo "Note: Could not create system folders (may need admin rights)"
    
    # Copy files with error handling
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/folder/16x16/folder.png"
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/icons/folder/16x16/folder.png"
        || ${CMAKE_COMMAND} -E echo "Note: Could not copy to system folder (resources will load from bundle)"
        
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/midi/16x16/midi.png"
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/icons/midi/16x16/midi.png"
        || ${CMAKE_COMMAND} -E echo "Note: Could not copy to system folder (resources will load from bundle)"
        
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/logo/logo.png"
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/logo/logo.png"
        || ${CMAKE_COMMAND} -E echo "Note: Could not copy to system folder (resources will load from bundle)"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/background/background.png"
        "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources/Resources/background/background.png"
        || ${CMAKE_COMMAND} -E echo "Note: Could not copy to system folder (resources will load from bundle)"
    
    COMMENT "Resources copied to system VST3 folder"
    VERBATIM
)

# Install resources to system VST3 folder
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/icons"
        DESTINATION "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources"
        OPTIONAL
        FILES_MATCHING PATTERN "*.png")

install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/logo"
        DESTINATION "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources"
        OPTIONAL
        FILES_MATCHING PATTERN "*.png")

install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/background"
        DESTINATION "C:/Program Files/Common Files/VST3/DrumGroovePro.vst3/Contents/Resources"
        OPTIONAL
        FILES_MATCHING PATTERN "*.png")

# Print build configuration summary
message(STATUS "DrumGroovePro Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  OpenGL: Enabled for hardware acceleration")
message(STATUS "  Targets: VST3 Plugin + Standalone Application")
message(STATUS "  DPI Aware: Yes")
message(STATUS "  Hardware Acceleration: Yes (OpenGL + DirectX)")