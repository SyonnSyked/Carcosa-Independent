#pragma once

namespace GAME
{
	struct Collidable {};
	struct Obstacle {};
	struct ToDestroy {};


	struct Transform
	{
		GW::MATH::GMATRIXF matrix;
	};

	struct Velocity
	{
		GW::MATH::GVECTORF vector;
	};

}
