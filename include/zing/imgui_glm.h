#pragma once

// Helpers for conversion
#include <glm/glm.hpp>
#include "math.h"

#define IM_VEC4_CLASS_EXTRA                                                 \
ImVec4(const glm::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }          \
ImVec4(const glm::ivec4& f) { x = (float)f.x; y = (float)f.y; z = (float)f.z; w = (float)f.w; }         \
operator glm::vec4() const { return glm::vec4(x,y,z,w); }                                               \
operator glm::ivec4() const { return glm::ivec4((int)x,(int)y,(int)z,(int)w); }                         \

#define IM_VEC2_CLASS_EXTRA                                                 \
ImVec2(const glm::vec2& f) { x = f.x; y = f.y; }                            \
ImVec2(const glm::ivec2& f) { x = (float)f.x; y = (float)f.y; }             \
operator glm::vec2() const { return glm::vec2(x, y); }                      \
operator glm::ivec2() const { return glm::ivec2((int)x, (int)y); }          \

#define IM_QUAT_CLASS_EXTRA                                                 \
ImQuat(const glm::quat& f) { x = f.x; y = f.y; z = f.z; w = f.w; }          \
operator glm::quat() const { return glm::quat(w,x,y,z); }

#define IM_VEC3_CLASS_EXTRA                                                             \
ImVec3(const glm::vec3& f) { x = f.x; y = f.y; z = f.z;}                                \
ImVec3(const glm::ivec3& f) { x = (float)f.x; y = (float)f.y; z = (float)f.z;}          \
operator glm::vec3() const { return glm::vec3(x,y,z); }                                 \
operator glm::ivec3() const { return glm::ivec3((int)x,(int)y,(int)z); }

#include <imgui.h>
