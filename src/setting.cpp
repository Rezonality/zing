#include <zing/file/toml_utils.h>
#include <toml++/toml.h>

#define DECLARE_SETTINGS
#include <zing/setting.h>

namespace Zing {
bool SettingManager::Save(const std::filesystem::path& path)
{
    toml::table tbl;

    for (const auto& [theme_name, values] : m_themes)
    {
        toml::table values_table;
        for (const auto& [value_name, value] : values)
        {
            switch (value.type)
            {
            case SettingType::Float:
                values_table.insert(value_name.ToString(), value.ToFloat());
                break;
            case SettingType::Bool:
                values_table.insert(value_name.ToString(), value.ToBool());
                break;
            case SettingType::Vec2f:
            {
                toml_write_vec2(values_table, value_name.ToString(), value.ToVec2f());
            }
            break;
            case SettingType::Vec3f:
            {
                toml_write_vec3(values_table, value_name.ToString(), value.ToVec3f());
            }
            break;
            case SettingType::Vec4f:
            {
                toml_write_vec4(values_table, value_name.ToString(), value.ToVec4f());
            }
            break;
            }
        }
        tbl.insert(theme_name, values_table);
    }

    std::ofstream fs(path, std::ios_base::trunc);
    fs << tbl;
    return true;
}

bool SettingManager::Load(const std::filesystem::path& path)
{
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(path.string());
        for (auto& [theme, value] : tbl)
        {
            if (value.is_table())
            {
                auto theme_table = value.as_table();
                if (theme_table)
                {
                    for (auto& [themeName, themeValue] : *theme_table)
                    {
                        if (themeValue.is_array())
                        {
                            auto name = std::string(themeName.str());
                            auto arr = themeValue.as_array();
                            switch (arr->size())
                            {
                            case 2:
                                Set(StringId(std::string(themeName.str())), toml_read_vec2(themeValue, glm::vec2(0.0f)));
                                break;
                            case 3:
                                Set(StringId(std::string(themeName.str())), toml_read_vec3(themeValue, glm::vec3(0.0f)));
                                break;
                            case 4:
                                Set(StringId(std::string(themeName.str())), toml_read_vec4(themeValue, glm::vec4(0.0f)));
                                break;
                            }
                        }
                        else if (themeValue.is_floating_point())
                        {
                            Set(StringId(std::string(themeName.str())), (float)themeValue.as_floating_point()->get());
                        }
                        else if (themeValue.is_boolean())
                        {
                            Set(StringId(std::string(themeName.str())), (bool)themeValue.as_boolean()->get());
                        }
                    }
                }
            }
        }
    }
    catch (const toml::parse_error&)
    {
        return false;
    }
    return true;
}

}