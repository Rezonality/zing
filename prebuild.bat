@echo off

echo %Time%

if not exist "vcpkg\vcpkg.exe" (
    cd vcpkg
    call bootstrap-vcpkg.bat -disableMetrics
    cd %~dp0
)

cd vcpkg
echo Installing Libraries
vcpkg install tinydir concurrentqueue kissfft portaudio stb date fmt clipp tomlplusplus glm sdl2[vulkan] imgui[sdl2-binding,freetype,vulkan-binding] magic-enum catch2 --triplet x64-windows-static-md --recurse
cd %~dp0
echo %Time%

