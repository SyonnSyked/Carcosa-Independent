#include "GameComponents.h"
#include "GameplaySystems.h"
#include <vector>
#include <algorithm>

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
    {
        // Give the player a temporary speed boost.
        auto& boost = registry.get_or_emplace<SpeedBoost>(player);

        // Make the player 50% faster for 10 seconds.
        boost.multiplier = 1.5f;
        boost.remaining = 10.0f;

        // Picking up another potion refreshes the timer.
        std::cout << "Speed Potion Collected! 10 Second Boost Activated!"
            << std::endl;

        break;
    }
    
    case BounsStatPickUpType::CDR:
        bonusStats.increaseCRD(pickup.Amount);
        break;
    case BounsStatPickUpType::CritChance:
        bonusStats.increaseCritChance(pickup.Amount);
        break;
    case BounsStatPickUpType::AttackSpeed:
        bonusStats.increaseAttackSpeed(pickup.Amount);
        break;
            case BounsStatPickUpType::HealthRestore:
            {
                // Make sure the player has a Health component.
                if (registry.all_of<Health>(player))
                {
                    auto& health = registry.get<Health>(player);

                    health.current = (std::min)(
                        health.current + pickup.Amount,
                        health.maximum
                        );

                    // Print a message so we know the potion worked.
                    std::cout << "Health Potion Collected! Current HP: "
                        << health.current << "/"
                        << health.maximum << std::endl;
                }

                break;
            }
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
