#include <algorithm>
#include <glm/glm.hpp>
#include <imgui.h>
#include "imgui_internal.h"

namespace ImGui
{
bool DragIntRange4(const char* label, glm::i32vec4& v, float v_speed, int v_min, int v_max, const char* format, const char* format_max)
{
    ImGuiContext* g = ImGui::GetCurrentContext();
    PushID(label);
    BeginGroup();
    PushMultiItemsWidths(4, CalcItemWidth());

    bool value_changed = DragInt("##amin", &v.x, v_speed, v_min, v_max, format);
    PopItemWidth();
    SameLine(0, g->Style.ItemInnerSpacing.x);
    v.y = std::max(v.x, v.y);
    value_changed |= DragInt("##bmin", &v.y, v_speed, std::min(v.x, v_max), v_max, format_max ? format_max : format);
    PopItemWidth();
    SameLine(0, g->Style.ItemInnerSpacing.x);
    v.z = std::max(v.y, v.z);
    value_changed |= DragInt("##cmin", &v.z, v_speed, std::min(v.y, v_max), v_max, format_max ? format_max : format);
    PopItemWidth();
    SameLine(0, g->Style.ItemInnerSpacing.x);
    v.w = std::max(v.z, v.w);
    value_changed |= DragInt("##dmin", &v.w, v_speed, std::min(v.z, v_max), v_max, format_max ? format_max : format);
    PopItemWidth();
    SameLine(0, g->Style.ItemInnerSpacing.x);

    ImGui::TextEx(label, FindRenderedTextEnd(label));
    EndGroup();
    PopID();

    return value_changed;
}
} // namespace ImGui