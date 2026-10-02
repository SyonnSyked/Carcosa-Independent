#pragma once
#include <cstdint>

namespace GAME {
struct Player {
    bool attackHeld = false;
};
struct Enemy {};
struct Health {
    float current, maximum;
};
struct Movement {
    float speed;
};
struct MeleeAttack {
    float damage, range, cooldown;
    float remaining = 0;
};
struct Class {
    std::string Type;
};

struct Collidable {};
struct Obstacle {};
struct ToDestroy {};
struct BonusStats {
    float BonusDamage = 0.0;
    float BonusHealth = 0.0;
    float BonusSpeed = 0.0;
    float CDR = 0.0;
    float CritChance = 0.0;
    float AttackSpeed = 0.0;

    void increaseDamage(float amount)
    {
        BonusDamage += amount;
    }

    void increaseHealth(float amount)
    {
        BonusHealth += amount;
    }

    void increaseSpeed(float amount)
    {
        BonusSpeed += amount;
    }

    void increaseCRD(float amount)
    {
        CDR += amount;
    }

    void increaseCritChance(float amount)
    {
        CritChance += amount;
    }

    void increaseAttackSpeed(float amount)
    {
        AttackSpeed += amount;
    }
};

enum class BounsStatPickUpType {
    BonusDamage,
    BonusHealth,
    BonusSpeed,
    CDR,
    CritChance,
    AttackSpeed,
};

struct PickUp {
    BounsStatPickUpType Type = BounsStatPickUpType::BonusDamage;
    float Amount = 0.0;
};

struct DamageFlash {
    float remaining;
};

struct DamageFeedbackConfig {
    bool enabled;
    float duration;
    float flashR, flashG, flashB;
};

struct HordeSpawner {
    bool enabled;
    float spawnInterval;
    float minimumRadius, maximumRadius;
    int maximumAlive;
    int totalSpawnLimit; // Zero means unlimited; counts only this spawner's creations.
    float elapsed = 0;
    std::uint64_t spawned = 0;
};

struct Transform {
    GW::MATH::GMATRIXF matrix;
};

struct Velocity {
    GW::MATH::GVECTORF vector;
};

struct CharacterConfig {
    // Initialization recipe, not live ECS state. Transform preserves exported scale/orientation.
    MeleeAttack attackData;
    Transform transform;

    std::string model;

    float health;
    float speed;
};

} // namespace GAME
