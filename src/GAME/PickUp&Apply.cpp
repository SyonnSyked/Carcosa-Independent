#include "GameComponents.h"
#include "GameplaySystems.h"
#include <vector>

namespace GAME
{
void ApplyPickUp(entt::registry &registry, entt::entity player, const PickUp &pickup)
{
    auto &bonusStats = registry.get_or_emplace<BonusStats>(player);
    switch (pickup.Type) {
    case BounsStatPickUpType::BonusDamage:
        bonusStats.increaseDamage(pickup.Amount);
        break;
    case BounsStatPickUpType::BonusHealth:
        bonusStats.increaseHealth(pickup.Amount);
        break;
    case BounsStatPickUpType::BonusSpeed:
        bonusStats.increaseSpeed(pickup.Amount);
        break;
    case BounsStatPickUpType::CDR:
        bonusStats.increaseCRD(pickup.Amount);
        break;
    case BounsStatPickUpType::CritChance:
        bonusStats.increaseCritChance(pickup.Amount);
        break;
    case BounsStatPickUpType::AttackSpeed:
        bonusStats.increaseAttackSpeed(pickup.Amount);
        break;
    }
}

void PickUpObject(entt::registry &registry, entt::entity player,
                  const GW::MATH::GVECTORF &playerPosition, float playerRadius)
{
    std::vector<entt::entity> consumed;
    const float collectionAllowance = registry.ctx().get<PickupConfig>().collectionAllowance;
    auto pickups = registry.view<Transform, PickUp>();
    pickups.each([&](entt::entity item, Transform &transform, PickUp &pickup) {
        const float offsetX = transform.matrix.row4.x - playerPosition.x;
        const float offsetZ = transform.matrix.row4.z - playerPosition.z;
        const float radius = playerRadius + collectionAllowance;
        if (offsetX * offsetX + offsetZ * offsetZ <= radius * radius) {
            ApplyPickUp(registry, player, pickup);
            consumed.push_back(item);
        }
    });
    // Defer destruction until iteration finishes, preserving pickup application order.
    for (auto item : consumed)
        registry.destroy(item);
}
} // namespace GAME
