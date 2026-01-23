# Blackheart

A JUCE-based VST3 audio effect plugin combining analog-style octave fuzz with chaotic digital pitch-shifting and modulation. Built for heavy, downtuned guitar and experimental sound design.

## Features

- **Fuzz Engine** - Analog-style nonlinear waveshaping with gain-dependent compression
- **Octave-Up Generation** - Rectification-based harmonic generation with dynamic gating
- **Pitch Shifting** - Momentary +1 and +2 octave shifts with granular synthesis
- **Chaos Modulation** - LFO, sample-and-hold, and envelope-responsive modulation
- **Low Latency** - Designed for real-time performance (<10ms)

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

## License

[Add license here]

## Acknowledgments

Initial prototype developed with AI assistance.
