# String Engine

A VST3 physically-modeled plucked string synthesizer inspired by Revitar2, built with JUCE. Based on the DSP code of Revitar2 maintained by SwooshyCueb (https://github.com/SwooshyCueb/Revitar2/tree/master)

Demo: [StringEngineTest2-SlideWorks.mp3](https://github.com/user-attachments/files/28430581/StringEngineTest2-SlideWorks.mp3)


<img width="1003" height="501" alt="image" src="https://github.com/user-attachments/assets/1001da35-882b-4bd3-923d-ac926a0b1234" />

## Features:
- Pick, pickup, and palm mute position across the entire length of the strings
- String visualizer
- Slides, vibrato, and hammer-ons/pulloffs via legato control
- Body resonance model
- Pick width, shape, centering, harmonics, noise, and strength
- Bridge damping model
- Palm mute model
- String stiffness model
- Tone control

## How to build:
### Requirements:
- JUCE Framework v8.0.12
- XCode or Visual Studio CE 2026 (CMake coming soon)

1. Open StringEngine.jucer in the Projucer
2. Select the exporter on the top
3. Launch the IDE
4. Clean the project
5. Hit Run

