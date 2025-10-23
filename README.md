# DrumGroovePro

**A  VST3 plugin for browsing, arranging, and exporting MIDI drum grooves with drum library remapping and multi-track timeline capabilities.**

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

###  **Intelligent Drum Remapping**
- **16 Supported Libraries**:
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
- **Seamless Conversion**: Drag a Superior Drummer groove onto a track set to EZdrummer—notes are automatically remapped!

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


---

##  Installation

### Quick Install (Windows)

1. **Download** the latest release from the [Releases page](https://github.com/InToEtherion/DrumGroovePro/releases)

2. **Extract** the ZIP file:
   ```
   DrumGroovePro_v0.9.0_Windows_x64.zip
   ```

3. **Copy** `DrumGroovePro.vst3` to your VST3 folder:
   ```
   C:\Program Files\Common Files\VST3\
   ```

4. **Restart** your DAW and scan for new plugins

5. **Load** DrumGroovePro as a MIDI effect on any track

---

##  Quick Start Guide

###  **Add Your Groove Library**

1. Click the **"+"** button in the Folder Panel (left side)
2. Browse to your MIDI groove folder
3. Select the source drum library (e.g., "Superior Drummer 3")
4. Click **"ADD TO LIBRARY"**

The plugin will scan all MIDI files in that folder and its subfolders.

---

### 2️⃣ **Browse and Preview Grooves**

1. Navigate through folders using the **Miller Columns browser** (center)
2. **Click** on a MIDI file to preview it
3. **Double-click** to dissect it into drum parts (Kick, Snare, Hi-Hat, etc.)
4. **Click** on individual drum parts to preview them

---

### 3️⃣ **Build Your Arrangement**

#### Option A: Drag to Timeline
1. **Drag** a MIDI file or drum part from the browser
2. **Drop** onto a track in the timeline (bottom)
3. The clip appears with visual MIDI preview

#### Option B: Drag to DAW
1. **Drag** a MIDI file from the browser (With "Control" Pressed), or "Control" + "Alt" when dragging from the timeline.
2. **Drop** directly onto a Reaper track 
3. MIDI file is imported at the current plugin BPM setting

---

### 4️⃣ **Adjust Tempo**

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

### 5️⃣ **Set Target Drum Library**

1. Use the **"Target Drum Library"** dropdown (top-right of browser)
2. Select your drum plugin (e.g., "EZdrummer")
3. All grooves and parts will automatically remap notes to match your drum library

**Example:**
- Drag a Superior Drummer 3 groove (kick on C1)
- Set target to EZdrummer (kick on C0)
- Notes are automatically remapped to C0!

---

### 6️⃣ **Export Your Work**

#### Save Timeline State:
1. Click **"File"** button → **"Save Timeline State"**
2. Choose a folder to save your project
3. All tracks, clips, BPM settings, and temporary files are saved

---

## 🔧 Building from Source

### Prerequisites

- **CMake** 3.22 or higher
- **Visual Studio 2022** (Windows)
- **JUCE** 8+ (included as submodule)
- **Git**

### Build Steps

```bash
# Clone repository
git clone https://github.com/InToEtherion/DrumGroovePro.git
cd DrumGroovePro

# Initialize JUCE submodule
git submodule update --init --recursive

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build (Release mode)
cmake --build . --config Release --parallel

# Plugin output location:
# build/DrumGroovePro_artefacts/Release/VST3/DrumGroovePro.vst3
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

## 🤝 Contributing

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

## 💬 Support

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

## 📜 License

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
