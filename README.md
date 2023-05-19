[Zing](https://github.com/Rezonality/zing) - A Simple Audio Integration Library
===================================================================================================
[![Builds](https://github.com/Rezonality/zing/workflows/Builds/badge.svg)](https://github.com/Rezonality/zing/actions?query=workflow%3ABuilds)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Rezonality/zing/blob/master/LICENSE)

Zing is an audio library designed to get ImGui Apps up and running with audio I/O.  It builds cross platform, but has so far only been tested on windows.  It's a grab-bag of useful stuff to get you started in audio.  The code is simple C++.  It needs a helper library submodule containing some useful c++ stuff.  You can probably drop this code into your app and start playing.

The demo lets you tweak parameters and generates a beep sound using soundpipe, in addition to the metronome that Ableton link provides (assuming you have sound output enabled).

- Configuration UI for the audio
- FFT analysis of the incoming audio in seperate threads, and simple display of the spectrum and audio waveform.
- Ableton Link integration, for sharing tempo with other applications.
- Portaudio for simple effect/audio processing modules.
- Simple thread profiler so you can see live what the UI and audio threads are doing.
- Save/Load of all settings to a TOML file.
- Management of the audio device using port audio.

![ImGui](screenshots/sample.png)

``` bash
git pull
git submodule update --init --recursive
prebuild.bat OR prebuild.sh
config.bat OR config.sh
build.bat OR build.sh
```

