#ifndef GAMECONFIG_H
#define GAMECONFIG_H

class GameConfig : public ini::IniFile
{ 
public:
	explicit GameConfig(const std::filesystem::path& path = "config/game.ini");
	~GameConfig() = default;
};

#endif
