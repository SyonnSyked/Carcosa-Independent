#include "CCL.h"

namespace
{
	std::list<std::function<void(entt::registry& reg)>>& GetComponentLogic()
	{
		// I keep this storage function-local to avoid cross-translation-unit initialization order issues.
		static std::list<std::function<void(entt::registry& reg)>> componentLogic;
		return componentLogic;
	}
}

namespace CCL {
	

	ComponentLogic::ComponentLogic(std::function<void(entt::registry& reg)> logic){
		GetComponentLogic().push_back(logic);
	}

	void InitializeComponentLogic(entt::registry& registry){
		for (auto& logic : GetComponentLogic()) {
			logic(registry);
		}
	}
}
