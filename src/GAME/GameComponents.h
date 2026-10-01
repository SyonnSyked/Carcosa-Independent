#pragma once

namespace GAME
{
    struct Player { bool attackHeld = false; };
    struct Enemy {};
    struct Health { float current, maximum; };
    struct Movement { float speed; };
    struct MeleeAttack {
        float damage, range, cooldown;
        float remaining = 0;
    };

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
