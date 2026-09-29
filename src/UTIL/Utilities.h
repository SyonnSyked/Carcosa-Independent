#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "GameConfig.h"

namespace UTIL
{
	struct Config
	{
		std::shared_ptr<GameConfig> gameConfig = std::make_shared<GameConfig>();
	};

	struct DeltaTime
	{
		double dtSec;
	};

	struct Input
	{
		GW::INPUT::GController gamePads;
		GW::INPUT::GInput immediateInput;
		GW::INPUT::GBufferedInput bufferedInput;
	};

}
#endif
