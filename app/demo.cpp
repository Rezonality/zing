#include <filesystem>
#include <fmt/format.h>
#include <memory>

extern "C" {
#include "libs/soundpipe/h/soundpipe.h"
}

#include <config_app.h>
using namespace Zing;
namespace fs = std::filesystem;

namespace {

void demo_draw()
{
    ImGui::Begin();

    ImGui::Text("Hello");

    ImGui::End();
}

void demo_cleanup()
{
}
