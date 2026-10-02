#pragma once
namespace GAME {
void InitializeDamageFeedback(entt::registry &registry);
void ActivateDamageFlash(entt::registry &registry, entt::entity entity);
void UpdateDamageFlash(entt::registry &registry, float deltaTime);
void UpdatePlayer(entt::registry &registry, float deltaTime);
void UpdateEnemy(entt::registry &registry, float deltaTime);
void UpdateCamera(entt::registry &registry);
void UpdateHordeSpawner(entt::registry &registry, float deltaTime);
void UpdateCombatTimers(entt::registry &registry, float deltaTime);
void TryMeleeAttack(entt::registry &registry, entt::entity attacker, entt::entity target);
void ApplyPickup(entt::registry &registry, entt::entity player, const PickUp& pickup);
void PickupObject(entt::registry &registry, entt::entity player, const GW::MATH::GVECTORF &playerPosition, float playerRadius);
float GroundDistance(entt::registry &registry, entt::entity a, entt::entity b);
} // namespace GAME
