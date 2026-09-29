#ifndef CCL_H_
#define CCL_H_


namespace CCL {

	


	struct ComponentLogic {
		ComponentLogic(std::function<void(entt::registry& reg)> logic);
	};


#define CCL_INTERNAL_EXPAND( x, y ) x##y
#define CCL_INTERNAL_COMBINE( left, right ) CCL_INTERNAL_EXPAND( left, right )
#define CCL_INTERNAL_CONNECT_COMPONENT_LOGIC( function ) \
        static void function( entt::registry& registry ); \
        namespace{ CCL::ComponentLogic CCL_INTERNAL_COMBINE( _StoreComponent, __COUNTER__ )( function ); } \
        static void function( entt::registry& registry )

#define CONNECT_COMPONENT_LOGIC() \
        CCL_INTERNAL_CONNECT_COMPONENT_LOGIC( CCL_INTERNAL_COMBINE( _DefineLogic, __COUNTER__ ) )


	void InitializeComponentLogic(entt::registry& registry); 
   
}
#endif
