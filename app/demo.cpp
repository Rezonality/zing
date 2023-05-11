#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <imgui.h>
#include <zing/audio/audio.h>
extern "C" {
#include <soundpipe/h/soundpipe.h>
}

#include <config_app.h>
using namespace Zing;
namespace fs = std::filesystem;

namespace {
}

void demo_init()
{
    audio_init(nullptr);
}

void demo_draw()
{
    audio_show_gui();    
}

void demo_cleanup()
{
    audio_destroy();
}
