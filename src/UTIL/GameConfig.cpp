#include "GameConfig.h"
#include <cmath>
#include <stdexcept>

GameConfig::GameConfig(const std::filesystem::path &path)
{
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Required project configuration was not found: " + path.string());
    load(path.string());
}

float GameConfig::RequireFloat(const std::string &section, const std::string &key) const
{
    try {
        const auto text = at(section).at(key).as<std::string>();
        std::size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value))
            throw std::invalid_argument("expected a finite number with no trailing text");
        return value;
    } catch (const std::exception &error) {
        throw std::runtime_error("Invalid required configuration " + section + "." + key + ": " +
                                 error.what());
    }
}
