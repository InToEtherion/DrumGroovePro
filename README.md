# DrumGroovePro

**A  VST3 plugin for browsing, arranging, and exporting MIDI drum grooves with drum library remapping and multi-track timeline capabilities.**

![DrumGroovePro Interface](images/plugin-interface.png)
---

##  Overview

DrumGroovePro MIDI drum groove workstation designed for producers, composers, and drummers. It provides an intuitive interface for browsing your MIDI groove library, dissecting patterns into individual drum parts (kick, snare, hi-hat, etc.), and arranging them on a multi-track timeline with per-track BPM control.

**Perfect for:**
-  Quickly auditioning drum grooves at different tempos
-  Building complex drum arrangements from individual parts
-  Converting grooves between different drum libraries (Superior Drummer, Addictive Drums, EZdrummer, etc.)
-  Exporting complete drum arrangements as MIDI files
-  Creating custom drum patterns by mixing and matching parts

---

##  Key Features

###  **Smart MIDI Browsing**
- **Miller Columns Interface**: Navigate your groove library with an intuitive, multi-column browser
- **Real-time Preview**: Click to preview grooves instantly at your project's tempo
- **MIDI Dissection**: Automatically splits grooves into drum parts (Kick, Snare, Hi-Hat, Toms, Cymbals, Percussion)
- **Drag & Drop**: Drag MIDI files directly into your DAW or onto the timeline

###  **Multi-Track Timeline**
- **Unlimited Tracks**: Create as many tracks as you need for complex arrangements
- **Per-Track BPM Control**: Each track can have its own tempo (60-400 BPM)
- **Visual MIDI Preview**: See note patterns directly on clips
- **Snap-to-Grid**: Precise clip positioning with adjustable grid intervals
- **Loop Regions**: Set loop points for arrangement workflow
- **Solo/Mute**: Standard track controls for playback management
- **Playhead Speed control**: Chnage the speed of playing to make further adjustments

###  **IVisual Latency Compensation**

- Adjustable from -200ms to 0ms
- Compensates for system/hardware audio latency
- Negative values make visual playhead lag behind audio (normal)
- Default: -20ms
- Controlled via Latency field in timeline controls

###  **Intelligent Drum Remapping**
- **18 Supported Libraries**:
 -  GeneralMIDI
 -  SuperiorDrummer3
 -  AddictiveDrums2
 -  Battery4 
 -  EZdrummer 
 -  GetGoodDrums 
 -  StevenSlateDrums 
 -  Ugritone
 -  BFD3
 -  MTPowerDrumKit2 
 -  DrumGizmo
 -  Sitala 
 -  KrimhDrums
 -  TheMonarchKit
 -  ShreddageDrums
 -  Damage2
 -  Triaz
 -  MODO Drum
 -  
**Seamless Conversion**: Drag a Superior Drummer groove onto a track set to EZdrummer—notes are automatically remapped.

###  **Project Management**
- **Save/Load Timeline State**: Save complete timeline arrangements with all tracks, clips, and BPM settings
- **Persistent Temporary Files**: Dissected drum parts are automatically saved with your project

---

##  System Requirements

### Windows
- **OS**: Windows 10 or Windows 11 (64-bit)
- **CPU**: Intel Core i5 / AMD Ryzen 5 or better
- **RAM**: 4 GB minimum, 8 GB recommended
- **Storage**: 50 MB for plugin installation
- **DAW**: Any VST3-compatible host (Reaper, FL Studio, Ableton Live, Cubase, Studio One, etc.)

### Linux
- **OS**: Tested on Arch Linu and Fedora (64-bit)
- **CPU**: Intel Core i5 / AMD Ryzen 5 or better
- **RAM**: 4 GB minimum, 8 GB recommended
- **Storage**: 50 MB for plugin installation
- **DAW**: Any VST3-compatible host (Reaper, Ardour, Carla, etc)

### MacOS

- **OS**: macOS 10.15 (Catalina) or later
- **CPU**: Intel Core i5 or Apple Silicon (M1/M2/M3/M4) or better
- **RAM**: 4 GB minimum, 8 GB recommended
- **Storage**: 50 MB for plugin installation
- **DAW**: Any VST3-compatible host (Logic Pro, GarageBand, Reaper, Ableton Live, Cubase, Studio One, etc.)

---

##  Installation

### Quick Install (Windows)

1. **Download** the latest release from the [Releases page](https://github.com/InToEtherion/DrumGroovePro/releases)

2. **Extract** the ZIP file:
   ```
   DrumGroovePro_vX.X.X_Windows_x64.zip
   ```

3. **Copy** `DrumGroovePro.vst3` to your VST3 folder:
   ```
   C:\Program Files\Common Files\VST3\
   ```

4. **Restart** your DAW and scan for new plugins

5. **Load** DrumGroovePro as a MIDI effect on any track


### Quick Install (Linux)

1. **Download** the latest release from the [Releases page](https://github.com/InToEtherion/DrumGroovePro/releases)

2. **Extract** the ZIP file:
   ```
   DrumGroovePro_vX.X.X_Linux_x64.zip
   ```

3. **Copy** `DrumGroovePro.vst3` to your VST3 folder:
   ```
   ~/.vst3/
   ```

4. **Restart** your DAW and scan for new plugins

5. **Load** DrumGroovePro as a MIDI effect on any track
---

##  Quick Start Guide

###  **Add Your Groove Library**

1. Click the **ADD FOLDER** button in the Folder Panel (left side)
2. Browse to your MIDI groove folder
3. Select the source drum library (e.g., "Superior Drummer 3")
4. Click **"ADD TO LIBRARY"**

The plugin will scan all MIDI files in that folder and its subfolders.

---

### **Browse and Preview Grooves**

1. Navigate through folders using the **Miller Columns browser** (center)
2. **Click** on a MIDI file to preview it
3. **Double-click** to dissect it into drum parts (Kick, Snare, Hi-Hat, etc.)
4. **Click** on individual drum parts to preview them

---

### **Build Your Arrangement**

#### Option A: Drag to Timeline
1. **Drag** a MIDI file or drum part from the browser
2. **Drop** onto a track in the timeline (bottom)
3. The clip appears with visual MIDI preview

#### Option B: Drag to DAW
1. **Drag** a MIDI file from the browser (With "Control" Pressed) or form Timeline with "control + Alt" pressed.
2. **Drop** directly onto a Reaper track 
3. MIDI file is imported at the current plugin BPM setting

---

### **Adjust Tempo**

#### Sync to Host:
- **Enable** "Sync to Host" in the header
- All playback follows your DAW's tempo

#### Manual BPM:
- **Disable** "Sync to Host"
- Use the **BPM slider** to set a custom tempo (60-400 BPM)

#### Per-Track BPM:
- Each track has its own **BPM control** in the track header
- Perfect for polyrhythmic arrangements or tempo experiments

---

### **Set Target Drum Library**

1. Use the **"Target Drum Library"** dropdown (top-right of browser)
2. Select your drum plugin (e.g., "EZdrummer")
3. All grooves and parts will automatically remap notes to match your drum library

**Example:**
- Drag a Superior Drummer 3 groove (kick on C1)
- Set target to EZdrummer (kick on C0)
- Notes are automatically remapped to C0!

---

### **Export Your Work**

#### Save Timeline State:
1. Click **"File"** button → **"Save Timeline State"**
2. Choose a folder to save your project
3. All tracks, clips, BPM settings, and temporary files are saved

#### Export MIDI as 1 file:
1. Click **"File"** button → **"Export as MIDI"**
2. Choose a folder to save your MIDI
3. All tracks, BPM settings are merged into 1 MIDI and is saved

#### Export MIDI, one MIDI peer track:
1. Click **"File"** button → **"Export Tracks as Separate MIDIs"**
2. Choose a folder to save your MIDI
3. All tracks, BPM settings are saved peer Track (You choose to mantain initial silence [if any])

---

## Building from Source

### Platform-Specific Requirements
### Windows

Visual Studio 2019 or later
Windows SDK

### macOS

Xcode 12 or later
macOS 10.13 or higher
For Universal Binary (Intel + Apple Silicon):

Xcode 12.2+ on macOS 11+ recommended

### Linux

GCC 9+ or Clang 10+
Development libraries:

# Ubuntu/Debian
  sudo apt-get install build-essential cmake libasound2-dev libjack-jackd2-dev \
      libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
      libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.0-dev \
      libglu1-mesa-dev mesa-common-dev
  
  # Fedora
  sudo dnf install cmake gcc-c++ alsa-lib-devel jack-audio-connection-kit-devel \
      freetype-devel libX11-devel libXcomposite-devel libXcursor-devel \
      libXext-devel libXinerama-devel libXrandr-devel libXrender-devel \
      webkit2gtk3-devel mesa-libGLU-devel
  
  # Arch Linux
  sudo pacman -S base-devel cmake alsa-lib jack freetype2 libx11 \
      libxcomposite libxcursor libxext libxinerama libxrandr \
      libxrender webkit2gtk glu

### Prerequisites 

- **CMake** 3.22 or higher
- **Visual Studio 2022** (Windows)
- **JUCE** 8+ (included as submodule)
- **Git**


### Build Steps

```bash
# Clone JUCE
git clone --recursive https://github.com/juce-framework/JUCE.git

# Clone repository
git clone https://github.com/InToEtherion/DrumGroovePro.git
cd DrumGroovePro

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build (Release mode)
cmake --build . --config Release --parallel

**MacOS**
Universal Binary Build (Intel + Apple Silicon)

# Configure with Universal Binary flag
cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" ..

# Build
cmake --build . --config Release --parallel
```

### Project Structure

```
DrumGroovePro/
├── Source/
│   ├── Core/               # MIDI processing and drum library logic
│   │   ├── MidiProcessor.cpp
│   │   ├── MidiDissector.cpp
│   │   └── DrumLibraryManager.cpp
│   ├── GUI/
│   │   ├── Components/     # UI components
│   │   │   ├── GrooveBrowser.cpp
│   │   │   ├── MultiTrackContainer.cpp
│   │   │   ├── Track.cpp
│   │   │   ├── TimelineManager.cpp
│   │   │   └── ...
│   │   └── LookAndFeel/    # Visual styling
│   ├── Utils/              # Utility functions
│   ├── PluginProcessor.cpp # Main plugin logic
│   └── PluginEditor.cpp    # Plugin UI root
├── Resources/              # Icons and assets
├── CMakeLists.txt          # Build configuration
└── README.md
```

---

## Contributing

Contributions are welcome! If you'd like to contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Reporting Issues

Found a bug? Please open an issue with:
- Detailed description of the problem
- Steps to reproduce
- Your system info (OS, DAW, plugin version)
- Screenshots if applicable

---

## Support

- **Issues**: [GitHub Issues](https://github.com/InToEtherion/DrumGroovePro/issues)
- **Discussions**: [GitHub Discussions](https://github.com/InToEtherion/DrumGroovePro/discussions)
- **Updates**: Check for updates in the plugin's About dialog

---

##  Acknowledgments

- **JUCE Framework**: For the powerful audio plugin framework

---

##  Support Development

If you find DrumGroovePro useful, consider supporting its development:

**[Buy Me a Coffee ☕](https://coff.ee/intoetherion)**

Your support helps maintain and improve the plugin!

---

## License

DrumGroovePro is licensed under the **GNU General Public License v3.0**.

This means you can:
- ✅ Use it for free (personal and commercial)
- ✅ Modify the source code
- ✅ Distribute modified versions

**But you must:**
- Share modifications under the same GPL v3 license
- Include the original license and copyright notice
- Make source code available if you distribute the plugin

See [LICENSE](LICENSE) for full details.
