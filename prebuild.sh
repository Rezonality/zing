#!/bin/bash -x
# Remember to preinstall linux libraries:
# (ibus,  tar, zip, unzip, buid-prerequisits, xorg-dev)
if [ ! -f "vcpkg/vcpkg" ]; then
    cd vcpkg
    ./bootstrap-vcpkg.sh -disableMetrics
    cd ..
fi

triplet=(x64-linux)
if [ "$(uname)" == "Darwin" ]; then
   triplet=(x64-osx)
fi

cd vcpkg
./vcpkg install ableton-link tinydir concurrentqueue stb portaudio date fmt clipp tomlplusplus glm sdl2[vulkan] imgui[sdl2-binding,freetype,vulkan-binding] magic-enum catch2 --triplet ${triplet[0]} --recurse
if [ "$(uname)" != "Darwin" ]; then
./vcpkg install glib --triplet ${triplet[0]} --recurse
fi
cd ..


