#pragma once
#include "GameComponents.h"
#include <entt/entt.hpp>
namespace GAME {
void InitializeDamageFeedback(entt::registry &registry);
void ActivateDamageFlash(entt::registry &registry, entt::entity entity);
void UpdateDamageFlash(entt::registry &registry, float deltaTime);
void UpdatePlayer(entt::registry &registry, float deltaTime);
void UpdateEnemy(entt::registry &registry, float deltaTime);
void UpdateProjectiles(entt::registry &registry, float deltaTime);
void UpdateSpellAreas(entt::registry &registry, float deltaTime);
void UpdateCamera(entt::registry &registry);
void UpdateHordeSpawner(entt::registry &registry, float deltaTime);
void TryWarriorAOE(entt::registry &registry, entt::entity player);
void UpdateCombatTimers(entt::registry &registry, float deltaTime);
void TryMeleeAttack(entt::registry &registry, entt::entity attacker, entt::entity target);
void ApplyPickUp(entt::registry &registry, entt::entity player, const PickUp &pickup);
void PickUpObject(entt::registry &registry, entt::entity player,
                  const GW::MATH::GVECTORF &playerPosition, float playerRadius);
float GroundDistance(entt::registry &registry, entt::entity a, entt::entity b);
} // namespace GAME

