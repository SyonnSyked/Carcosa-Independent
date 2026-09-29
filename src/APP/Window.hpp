namespace APP {

struct WindowClosed {};


struct Window {
    int x = 0, y = 0, width = 100, height = 100;
    GW::SYSTEM::GWindowStyle style = GW::SYSTEM::GWindowStyle::WINDOWEDBORDERED;
    std::string title = "Unitialized";
};



inline void Construct_Window(entt::registry &registry, entt::entity entity)
{


    auto &window = registry.get<Window>(entity);
    GW::SYSTEM::GWindow newWindow;
    if (+newWindow.Create(window.x, window.y, window.width, window.height, window.style)) {
        newWindow.SetWindowName(window.title.c_str());
        registry.emplace<GW::SYSTEM::GWindow>(entity, newWindow.Relinquish());
    } else
        std::cout << "Failed to create window" << std::endl;
}


inline void Update_Window(entt::registry &registry, entt::entity entity)
{


    auto &win = registry.get<GW::SYSTEM::GWindow>(entity);
    if (-win.ProcessWindowEvents()) {
        registry.emplace_or_replace<WindowClosed>(entity);
    }

    else if (registry.any_of<DRAW::VulkanRenderer>(entity)) {

        registry.patch<DRAW::VulkanRenderer>(entity);
    }
}


CONNECT_COMPONENT_LOGIC()
{

    registry.on_construct<Window>().connect<Construct_Window>();
    registry.on_update<Window>().connect<Update_Window>();
}
}
