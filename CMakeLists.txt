cmake_minimum_required(VERSION 3.22)
project(DrumGroovePro VERSION 0.9.4)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE - using relative path as requested (no hardcoded paths)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE juce_build)

# Platform-specific plugin formats
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

# Add the plugin target
juce_add_plugin(DrumGroovePro
    PRODUCT_NAME "DrumGroovePro"
    COMPANY_NAME "InToEtherion"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE InTE
    PLUGIN_CODE Dgp1
    FORMATS ${PLUGIN_FORMATS}
    VST3_CATEGORIES "Fx" "Tools"
    DESCRIPTION "MIDI Effect - Drum Groove Manager and MIDI Processor"
    VST3_AUTO_MANIFEST FALSE
    COMPANY_WEBSITE "https://intoetherion.com"
    COMPANY_EMAIL "info@intoetherion.com"
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

# Base compile definitions - Common to all platforms
target_compile_definitions(DrumGroovePro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_ENABLE_REPAINT_DEBUGGING=0
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

#==============================================================================
# WINDOWS-SPECIFIC SETTINGS
#==============================================================================
if(WIN32)
    target_compile_definitions(DrumGroovePro PUBLIC
        NOMINMAX
        UNICODE
        _UNICODE
        WINVER=0x0A00          # Windows 10/11
        _WIN32_WINNT=0x0A00    # Windows 10/11
        WIN32_LEAN_AND_MEAN
        JUCE_USE_DIRECTWRITE=1
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

#==============================================================================
# macOS-SPECIFIC SETTINGS
#==============================================================================
elseif(APPLE)
    # macOS version targeting
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

    # UNIVERSAL BINARY: Build for both Intel (x86_64) and Apple Silicon (arm64)
    # This creates a "fat" binary that works on both architectures
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")

    target_compile_definitions(DrumGroovePro PUBLIC
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
    )

    # macOS compiler flags for Clang
    target_compile_options(DrumGroovePro PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-deprecated-declarations
    )

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(DrumGroovePro PRIVATE
            -O3
            -ffast-math
            -fvectorize
            -fslp-vectorize
        )
    endif()

    # Code signing (set to automatic - user can configure later)
    set_target_properties(DrumGroovePro PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    )

#==============================================================================
# LINUX-SPECIFIC SETTINGS (PipeWire + XWayland)
#==============================================================================
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(DrumGroovePro PUBLIC
        JUCE_JACK=1              # JACK audio support
        JUCE_ALSA=1              # ALSA support (fallback)
        JUCE_USE_X11=1           # X11 for XWayland compatibility
        LINUX=1
        # CRITICAL: Text rendering workarounds for JUCE 8.0.4 Linux
        JUCE_USE_FREETYPE=1
        JUCE_USE_FREETYPE_AMALGAMATED=0
        # CRITICAL: Enable OpenGL on Linux for smooth timeline playhead animation
        JUCE_OPENGL=1
    )

    # Linux compiler flags for GCC/Clang
    target_compile_options(DrumGroovePro PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -fPIC                    # Position independent code (required for shared libs)
    )

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(DrumGroovePro PRIVATE
            -O3
            -ffast-math
            -march=native
            -mtune=native
        )
    endif()
endif()

#==============================================================================
# JUCE MODULE LINKING - Common to all platforms
#==============================================================================
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

#==============================================================================
# PLATFORM-SPECIFIC LIBRARIES
#==============================================================================

if(WIN32)
    # Windows OpenGL and DirectX libraries
    target_link_libraries(DrumGroovePro PRIVATE
        opengl32
        glu32
        d3d11      # DirectX for hardware acceleration
        dxgi       # DirectX Graphics Infrastructure
    )

elseif(APPLE)
    # macOS frameworks required by JUCE
    target_link_libraries(DrumGroovePro PRIVATE
        "-framework Accelerate"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework Carbon"
        "-framework Cocoa"
        "-framework CoreAudio"
        "-framework CoreAudioKit"
        "-framework CoreMIDI"
        "-framework IOKit"
        "-framework QuartzCore"
        "-framework WebKit"
        "-framework Metal"
        "-framework MetalKit"
        "-framework OpenGL"
    )

elseif(UNIX AND NOT APPLE)
    # Find required Linux packages
    find_package(PkgConfig REQUIRED)

    # ALSA (Linux audio - required)
    pkg_check_modules(ALSA REQUIRED alsa)
    target_include_directories(DrumGroovePro PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(DrumGroovePro PRIVATE ${ALSA_LIBRARIES})

    # JACK audio (recommended)
    pkg_check_modules(JACK jack)
    if(JACK_FOUND)
        target_include_directories(DrumGroovePro PRIVATE ${JACK_INCLUDE_DIRS})
        target_link_libraries(DrumGroovePro PRIVATE ${JACK_LIBRARIES})
        message(STATUS "JACK audio support: Enabled")
    else()
        message(STATUS "JACK audio support: Not found (optional)")
    endif()

    # PipeWire support (modern Linux audio - optional but recommended)
    pkg_check_modules(PIPEWIRE libpipewire-0.3)
    if(PIPEWIRE_FOUND)
        target_include_directories(DrumGroovePro PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
        target_link_libraries(DrumGroovePro PRIVATE ${PIPEWIRE_LIBRARIES})
        target_compile_definitions(DrumGroovePro PRIVATE JUCE_PIPEWIRE=1)
        message(STATUS "PipeWire audio support: Enabled")
    else()
        message(STATUS "PipeWire audio support: Not found (optional, ALSA will be used)")
    endif()

    # X11 libraries (required for XWayland compatibility)
    find_package(X11 REQUIRED)
    target_link_libraries(DrumGroovePro PRIVATE
        ${X11_LIBRARIES}
        ${X11_Xext_LIB}
        ${X11_Xinerama_LIB}
        ${X11_Xrandr_LIB}
        ${X11_Xcursor_LIB}
    )

    # FreeType for font rendering
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    target_include_directories(DrumGroovePro PRIVATE ${FREETYPE_INCLUDE_DIRS})
    target_link_libraries(DrumGroovePro PRIVATE ${FREETYPE_LIBRARIES})

    # Additional Linux libraries
    target_link_libraries(DrumGroovePro PRIVATE
        pthread           # Threading
        dl               # Dynamic loading
        rt               # Real-time extensions
        GL               # OpenGL
    )
endif()

#==============================================================================
# OUTPUT DIRECTORIES
#==============================================================================
set_target_properties(DrumGroovePro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

#==============================================================================
# RESOURCE PATHS - Platform-specific
#==============================================================================
if(WIN32)
    set(BUILD_VST3_RESOURCES "${CMAKE_BINARY_DIR}/lib/VST3/DrumGroovePro.vst3/Contents/Resources")
    set(SYSTEM_VST3_RESOURCES "$ENV{COMMONPROGRAMFILES}/VST3/DrumGroovePro.vst3/Contents/Resources")
elseif(APPLE)
    set(BUILD_VST3_RESOURCES "${CMAKE_BINARY_DIR}/VST3/DrumGroovePro.vst3/Contents/Resources")
    set(SYSTEM_VST3_RESOURCES "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/DrumGroovePro.vst3/Contents/Resources")
    set(BUILD_AU_RESOURCES "${CMAKE_BINARY_DIR}/AU/DrumGroovePro.component/Contents/Resources")
    set(SYSTEM_AU_RESOURCES "$ENV{HOME}/Library/Audio/Plug-Ins/Components/DrumGroovePro.component/Contents/Resources")
elseif(UNIX AND NOT APPLE)
    set(BUILD_VST3_RESOURCES "${CMAKE_BINARY_DIR}/VST3/DrumGroovePro.vst3/Contents/Resources")
    set(SYSTEM_VST3_RESOURCES "$ENV{HOME}/.vst3/DrumGroovePro.vst3/Contents/Resources")
endif()

#==============================================================================
# FIX MODULEINFO.JSON - Remove trailing commas (JUCE bug workaround)
#==============================================================================
add_custom_command(TARGET DrumGroovePro_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "=== Fixing moduleinfo.json trailing commas ==="
    COMMAND sed -i 's/false,$/false/g' "${BUILD_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND sed -i 's/true,$/true/g' "${BUILD_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND sed -i 's/\\],$/\\]/g' "${BUILD_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND sed -i 's/},$/}/g' "${BUILD_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND ${CMAKE_COMMAND} -E echo "=== moduleinfo.json fixed ==="
    COMMENT "Fixing JUCE-generated moduleinfo.json trailing commas"
    VERBATIM
)

#==============================================================================
# RESOURCE COPYING - VST3 Build Directory
#==============================================================================
add_custom_command(TARGET DrumGroovePro_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "=== Copying resources to VST3 bundle ==="

    # Create directory structure in build folder
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_VST3_RESOURCES}/icons/folder/16x16"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_VST3_RESOURCES}/icons/midi/16x16"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_VST3_RESOURCES}/logo"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_VST3_RESOURCES}/background"

    # Copy icon files to build folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/folder/16x16/folder.png"
        "${BUILD_VST3_RESOURCES}/icons/folder/16x16/folder.png"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/midi/16x16/midi.png"
        "${BUILD_VST3_RESOURCES}/icons/midi/16x16/midi.png"

    # Copy logo to build folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/logo/logo.png"
        "${BUILD_VST3_RESOURCES}/logo/logo.png"

    # Copy background to build folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/background/background.png"
        "${BUILD_VST3_RESOURCES}/background/background.png"

    COMMAND ${CMAKE_COMMAND} -E echo "=== Resources copied to build directory ==="
    COMMAND ${CMAKE_COMMAND} -E echo "Build VST3 path: ${BUILD_VST3_RESOURCES}"

    COMMENT "Copying resources to build VST3 bundle"
    VERBATIM
)

#==============================================================================
# RESOURCE COPYING - VST3 System Installation
#==============================================================================
add_custom_command(TARGET DrumGroovePro_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "=== Copying resources to system VST3 folder ==="

    # Create directories in system folder
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_VST3_RESOURCES}/icons/folder/16x16" || ${CMAKE_COMMAND} -E true
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_VST3_RESOURCES}/icons/midi/16x16" || ${CMAKE_COMMAND} -E true
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_VST3_RESOURCES}/logo" || ${CMAKE_COMMAND} -E true
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_VST3_RESOURCES}/background" || ${CMAKE_COMMAND} -E true

    # Copy files to system folder
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/folder/16x16/folder.png"
        "${SYSTEM_VST3_RESOURCES}/icons/folder/16x16/folder.png" || ${CMAKE_COMMAND} -E true

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/icons/midi/16x16/midi.png"
        "${SYSTEM_VST3_RESOURCES}/icons/midi/16x16/midi.png" || ${CMAKE_COMMAND} -E true

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/logo/logo.png"
        "${SYSTEM_VST3_RESOURCES}/logo/logo.png" || ${CMAKE_COMMAND} -E true

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Resources/background/background.png"
        "${SYSTEM_VST3_RESOURCES}/background/background.png" || ${CMAKE_COMMAND} -E true

    # Fix moduleinfo.json in system folder too
    COMMAND sed -i 's/false,$/false/g' "${SYSTEM_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND sed -i 's/true,$/true/g' "${SYSTEM_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND sed -i 's/\\],$/\\]/g' "${SYSTEM_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true
    COMMAND sed -i 's/},$/}/g' "${SYSTEM_VST3_RESOURCES}/moduleinfo.json" || ${CMAKE_COMMAND} -E true

    COMMAND ${CMAKE_COMMAND} -E echo "=== Resources copied to system folder ==="
    COMMAND ${CMAKE_COMMAND} -E echo "System VST3 path: ${SYSTEM_VST3_RESOURCES}"

    COMMENT "Copying resources to system VST3 folder"
    VERBATIM
)

#==============================================================================
# RESOURCE COPYING - AudioUnit (macOS only)
#==============================================================================
if(APPLE)
    add_custom_command(TARGET DrumGroovePro_AU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=== Copying resources to AU bundle ==="

        # Create directory structure in AU build folder
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_AU_RESOURCES}/icons/folder/16x16"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_AU_RESOURCES}/icons/midi/16x16"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_AU_RESOURCES}/logo"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_AU_RESOURCES}/background"

        # Copy files to AU build folder
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/icons/folder/16x16/folder.png"
            "${BUILD_AU_RESOURCES}/icons/folder/16x16/folder.png"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/icons/midi/16x16/midi.png"
            "${BUILD_AU_RESOURCES}/icons/midi/16x16/midi.png"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/logo/logo.png"
            "${BUILD_AU_RESOURCES}/logo/logo.png"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/background/background.png"
            "${BUILD_AU_RESOURCES}/background/background.png"

        # Copy to system AU folder
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_AU_RESOURCES}/icons/folder/16x16" || ${CMAKE_COMMAND} -E true
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_AU_RESOURCES}/icons/midi/16x16" || ${CMAKE_COMMAND} -E true
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_AU_RESOURCES}/logo" || ${CMAKE_COMMAND} -E true
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSTEM_AU_RESOURCES}/background" || ${CMAKE_COMMAND} -E true

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/icons/folder/16x16/folder.png"
            "${SYSTEM_AU_RESOURCES}/icons/folder/16x16/folder.png" || ${CMAKE_COMMAND} -E true

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/icons/midi/16x16/midi.png"
            "${SYSTEM_AU_RESOURCES}/icons/midi/16x16/midi.png" || ${CMAKE_COMMAND} -E true

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/logo/logo.png"
            "${SYSTEM_AU_RESOURCES}/logo/logo.png" || ${CMAKE_COMMAND} -E true

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Resources/background/background.png"
            "${SYSTEM_AU_RESOURCES}/background/background.png" || ${CMAKE_COMMAND} -E true

        COMMAND ${CMAKE_COMMAND} -E echo "=== Resources copied to AU folders ==="

        COMMENT "Copying resources to AU bundles"
        VERBATIM
    )
endif()

#==============================================================================
# BUILD CONFIGURATION SUMMARY
#==============================================================================
message(STATUS "")
message(STATUS "======================================================")
message(STATUS "DrumGroovePro Build Configuration:")
message(STATUS "======================================================")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Plugin Formats: ${PLUGIN_FORMATS}")
message(STATUS "  OpenGL: Enabled")

if(APPLE)
    message(STATUS "  macOS Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "  AudioUnit Support: Yes")
endif()

if(UNIX AND NOT APPLE)
    if(PIPEWIRE_FOUND)
        message(STATUS "  PipeWire: Enabled")
    endif()
    if(JACK_FOUND)
        message(STATUS "  JACK Audio: Enabled")
    endif()
    message(STATUS "  ALSA: Enabled")
    message(STATUS "  X11 (XWayland): Enabled")
endif()

message(STATUS "  Resource Source: ${CMAKE_SOURCE_DIR}/Resources")
message(STATUS "======================================================")
message(STATUS "")
