#include <glm/glm.hpp>

#pragma once

namespace ImGui
{
bool DragIntRange4(const char* label, glm::i32vec4& v, float v_speed, int v_min, int v_max, const char* format = nullptr, const char* format_max = nullptr);
}
