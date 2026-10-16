#pragma once
#include "GameComponents.h"
#include <entt/entt.hpp>
namespace GAME {
void InitializeDamageFeedback(entt::registry &registry);
void ActivateDamageFlash(entt::registry &registry, entt::entity entity);
void UpdateDamageFlash(entt::registry &registry, float deltaTime);
void UpdatePlayer(entt::registry &registry, float deltaTime);
void MovePlayerWithCollision(entt::registry &registry, entt::entity player, float x, float z);
void UpdateEnemy(entt::registry &registry, float deltaTime);
void UpdateProjectiles(entt::registry &registry, float deltaTime);
void UpdateSpellAreas(entt::registry &registry, float deltaTime);
void UpdateVampireQueen(entt::registry &registry, float deltaTime);
void UpdateCamera(entt::registry &registry);
void UpdateHordeSpawner(entt::registry &registry, float deltaTime);
void TryWarriorAOE(entt::registry &registry, entt::entity player);
void TryThrowAxe(entt::registry &registry, entt::entity player,
                 const GW::MATH::GVECTORF &target);
void UpdateThrownAxes(entt::registry &registry, float deltaTime);
void UpdateCombatTimers(entt::registry &registry, float deltaTime);
void TryMeleeAttack(entt::registry &registry, entt::entity attacker, entt::entity target);
void HandleEntityDeath(entt::registry &registry);
void ApplyPickUp(entt::registry &registry, entt::entity player, const PickUp &pickup);
void PickUpObject(entt::registry &registry, entt::entity player,
                  const GW::MATH::GVECTORF &playerPosition, float playerRadius);
void DropDungeonKey(entt::registry &registry, const Transform placement);
void InitializeDungeonKeys(entt::registry &registry);
void CollectDungeonKey(entt::registry &registry);
void UseDungeonKey(entt::registry &registry);

void TryVampiricDrain(entt::registry &registry, entt::entity target);
} // namespace GAME
