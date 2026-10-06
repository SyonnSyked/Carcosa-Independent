#pragma once
#include <Gateware.h>
#include <cstdint>
#include <string>
#include <vector>

namespace GAME {

struct Transform {
    GW::MATH::GMATRIXF matrix;
};

struct Inventory {
    std::vector<entt::entity> items;
};

struct Player {
    bool attackHeld = false;
    bool abilityHeld = false;

    Transform location;

    Inventory inventory;
};
struct WarriorAOE {
    float damage = 35.0f;
    float radius = 3.0f;
    float cooldown = 3.0f;
    float remaining = 0.0f;
};
struct Enemy {
    bool isBoss;

    int whichKeyToDrop;
};

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

struct VampireQueen {};

enum class BounsStatPickUpType {
    BonusDamage,
    BonusHealth,
    BonusSpeed,
    CDR,
    CritChance,
    AttackSpeed,
};

struct DungeonKey {
    int level;

    float distToUse;
};

struct PickUp {
    BounsStatPickUpType Type = BounsStatPickUpType::BonusDamage;
    float Amount = 0.0;
};

struct PickupConfig {
    float collectionAllowance; // World units added to the caller's player radius.
};

struct GameplayCameraConfig {
    GW::MATH::GVECTORF offset;
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

struct DungeonExit {
    Transform location;
};

} // namespace GAME
