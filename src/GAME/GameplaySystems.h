#pragma once
namespace GAME {
void UpdatePlayer(entt::registry& registry, float deltaTime);
void UpdateEnemy(entt::registry& registry, float deltaTime);
void UpdateCamera(entt::registry& registry);
void UpdateCombatTimers(entt::registry& registry, float deltaTime);
void TryMeleeAttack(entt::registry& registry, entt::entity attacker, entt::entity target);
float GroundDistance(entt::registry& registry, entt::entity a, entt::entity b);
}
