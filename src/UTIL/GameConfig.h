#ifndef GAMECONFIG_H
#define GAMECONFIG_H
#include <filesystem>
#include <inicpp.h>

class GameConfig : public ini::IniFile
{
  public:
    explicit GameConfig(const std::filesystem::path &path = "config/game.ini");
    ~GameConfig() = default;

    float RequireFloat(const std::string &section, const std::string &key) const;
};

#endif
