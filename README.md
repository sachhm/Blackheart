# Blackheart

A JUCE-based VST3 audio effect plugin combining analog-style octave fuzz with chaotic digital pitch-shifting and modulation. Designed for heavy, downtuned guitar and experimental sound design.

## Project Overview

The initial prototype of Blackheart was developed with AI assistance. I am now rewriting all core DSP components from scratch to practice my own signal processing design and implementation skills. This is primarily a learning project to explore DSP concepts and real-time audio processing.

## Planned DSP Rewrite / To-Do
- [ ] Re-implement the **Fuzz Engine** using custom nonlinear waveshaping and gain-dependent compression
- [ ] Rewrite **Octave-Up Generation** with improved rectification and dynamic gating
- [ ] Develop a **Granular Pitch Shifter** from scratch for +1 and +2 octave shifts
- [ ] Implement **Chaos Modulation** using LFOs, sample-and-hold, and envelope-controlled modulation
- [ ] Optimize **low-latency performance** (<10ms) for real-time usage
- [ ] Experiment with **unit tests and signal flow documentation** for each DSP component

## Features

- **Fuzz Engine** – Analog-style nonlinear waveshaping with gain-dependent compression  
- **Octave-Up Generation** – Rectification-based harmonic generation with dynamic gating  
- **Pitch Shifting** – Momentary +1 and +2 octave shifts using granular synthesis  
- **Chaos Modulation** – LFO, sample-and-hold, and envelope-responsive modulation  
- **Low Latency** – Optimized for real-time performance (<10ms)

## Parameters

| Parameter | Description |
|-----------|-------------|
| Gain | Distortion intensity |
| Glare | Octave-up amount + gate interaction |
| Blend | Dry/wet mix |
| Level | Output level of fuzz block |
| Speed | Modulation rate |
| Chaos | Modulation depth + randomness |
| Rise | Pitch shift attack time |
| Octave +1 | Momentary +1 octave pitch shift |
| Octave +2 | Momentary +2 octave pitch shift |

## Building

### Requirements

- [JUCE Framework](https://juce.com/) (v7.x recommended)  
- C++17 compatible compiler  
- CMake 3.15+ or Projucer  

### Build with Projucer

1. Open `Blackheart.jucer` in Projucer  
2. Export to your IDE (Xcode, Visual Studio, etc.)  
3. Build the generated project  


## Acknowledgments

- Initial prototype developed with AI assistance  
- Built using the [JUCE Framework](https://juce.com/)  

## License

AGPLv3 (Same license that JUCE uses for Open Source projects)
