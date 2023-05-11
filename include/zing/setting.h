#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <unordered_map>
#include <zing/string/string_utils.h>

namespace Zing {

enum class SettingType
{
    Unknown,
    Float,
    Vec2f,
    Vec3f,
    Vec4f,
    Bool
};

struct SettingValue
{
    SettingValue()
        : type(SettingType::Unknown)
        , f4(glm::vec4(0.0f))
    {
    }
    SettingValue(const glm::vec4& val)
        : type(SettingType::Vec4f)
        , f4(val)
    {
    }
    SettingValue(const glm::vec3& val)
        : type(SettingType::Vec3f)
        , f3(val)
    {
    }
    SettingValue(const glm::vec2& val)
        : type(SettingType::Vec2f)
        , f2(val)
    {
    }
    SettingValue(const float& val)
        : type(SettingType::Float)
        , f(val)
    {
    }
    SettingValue(const bool& val)
        : type(SettingType::Bool)
        , b(val)
    {
    }

    glm::vec4 ToVec4f() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec4f;
        }

        if (type == SettingType::Vec4f)
        {
            return f4;
        }
        return glm::vec4(f);
    }

    glm::vec2 ToVec2f() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec2f;
        }

        switch (type)
        {
        case SettingType::Vec2f:
            return f2;
            break;
        case SettingType::Vec3f:
            return glm::vec2(f3);
            break;
        case SettingType::Vec4f:
            return glm::vec2(f4);
            break;
        case SettingType::Float:
            return glm::vec2(f);
            break;
        }
        return glm::vec2(0.0f);
    }

    glm::vec3 ToVec3f() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec3f;
        }

        switch (type)
        {
        case SettingType::Vec2f:
            return glm::vec3(f2.x, f2.y, 0.0f);
            break;
        case SettingType::Vec3f:
            return f3;
            break;
        case SettingType::Vec4f:
            return glm::vec3(f4);
            break;
        case SettingType::Float:
            return glm::vec3(f);
            break;
        }
        return glm::vec3(0.0f);
    }

    float ToFloat() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Float;
        }

        if (type == SettingType::Float)
        {
            return f;
        }
        return f4.x;
    }

    bool ToBool() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Bool;
        }

        if (type == SettingType::Bool)
        {
            return b;
        }

        return f4.x > 0.0f ? true : false;
    }
    union
    {
        glm::vec4 f4 = glm::vec4(1.0f);
        glm::vec3 f3;
        glm::vec2 f2;
        float f;
        bool b;
    };
    mutable SettingType type;
};

using SettingMap = std::unordered_map<StringId, SettingValue>;
class SettingManager
{
public:

    bool Save(const std::filesystem::path& path);
    bool Load(const std::filesystem::path& path);
    void Set(const StringId& id, const SettingValue& value)
    {
        auto& slot = m_themes[m_currentSetting][id];
        slot = value;
    }

    const SettingValue& Get(const StringId& id)
    {
        auto& theme = m_themes[m_currentSetting];
        return theme[id];
    }

    float GetFloat(const StringId& id)
    {
        auto& theme = m_themes[m_currentSetting];
        return theme[id].ToFloat();
    }

    glm::vec2 GetVec2f(const StringId& id)
    {
        auto& theme = m_themes[m_currentSetting];
        return theme[id].ToVec2f();
    }

    glm::vec4 GetVec4f(const StringId& id)
    {
        auto& theme = m_themes[m_currentSetting];
        return theme[id].ToVec4f();
    }
    
    bool GetBool(const StringId& id)
    {
        auto& theme = m_themes[m_currentSetting];
        return theme[id].ToBool();
    }

    std::unordered_map<std::string, SettingMap> m_themes;
    std::string m_currentSetting = "Default Setting";
};

#ifdef DECLARE_SETTINGS
#define DECLARE_SETTING_VALUE(name) Zing::StringId name(#name);
#else
#define DECLARE_SETTING_VALUE(name) extern Zing::StringId name;
#endif

class GlobalSettingManager : public SettingManager
{
public:
    static GlobalSettingManager& Instance()
    {
        static GlobalSettingManager setting;
        return setting;
    }
};

// Grid
DECLARE_SETTING_VALUE(s_windowSize);
DECLARE_SETTING_VALUE(b_windowMaximized);

}
