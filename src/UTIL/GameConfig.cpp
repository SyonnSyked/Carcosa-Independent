#include "GameConfig.h"
#include <stdexcept>

GameConfig::GameConfig(const std::filesystem::path& path)
{
	if (!std::filesystem::is_regular_file(path))
		throw std::runtime_error("Required project configuration was not found: " + path.string());
	load(path.string());
}
