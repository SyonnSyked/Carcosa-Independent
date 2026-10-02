
#pragma once
#include "GameComponents.h"
#include "Gateware.h"
#include <entt/entt.hpp> 
// picking up bonuses and applying them
namespace GAME
{

    inline void ApplyPickUp(entt::registry& reg, entt::entity player, const PickUp& p)
    {
        auto& ModdingStats = reg.get_or_emplace<BonusStats>(player);

        switch (p.Type)
        {
        case BounsStatPickUpType::BonusDamage:
            ModdingStats.increaseDamage(p.Amount);
            break;
        case BounsStatPickUpType::BonusHealth:    
            ModdingStats.increaseHealth(p.Amount);
            break;
        case BounsStatPickUpType::BonusSpeed:
            ModdingStats.increaseSpeed(p.Amount);
            break;
        case BounsStatPickUpType::CDR:
            ModdingStats.increaseCRD(p.Amount);
            break;
        case BounsStatPickUpType::CritChance:
            ModdingStats.increaseCritChance(p.Amount);
            break;
        case BounsStatPickUpType::AttackSpeed:
            ModdingStats.increaseAttackSpeed(p.Amount);
            break;        
        }

    }



    inline void PickUp(entt::registry& reg, entt::entity player, const GW::MATH::GVECTORF& playerposition,float playerradius)
    {
        std::vector<entt::entity> consumed;

        auto view = reg.view<Transform, PickUp>();
        view.each([&](entt::entity item, transform& t, PickUp& p))
        {
            float itemX = t.matrix.row4.x - playerposition.x;
            float itemY = t.matrix.row4.y - playerposition.y;
            float radius = playerradius+0.5f;

            if(itemX * itemX + itemY * itemY <= r*r)
            {
                ApplyPickUp(reg,player,p);
                consumed.push_back(item);
            }

        }

        for(auto item : consumed)
            reg.destroy(item);

    }


}


