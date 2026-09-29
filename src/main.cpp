#include "CCL.h"
#include "DRAW/DrawComponents.h"
#include "APP/Window.hpp"
#include "GAME/GameManager.h"
#include "UTIL/Utilities.h"

int main()
{
    try {
        entt::registry registry;
        CCL::InitializeComponentLogic(registry);

        auto& config = registry.ctx().emplace<UTIL::Config>();
        registry.ctx().emplace<UTIL::DeltaTime>();
        const auto& settings = *config.gameConfig;

        const auto display = registry.create();
        registry.emplace<APP::Window>(display, APP::Window{
            settings.at("Window").at("xstart").as<int>(),
            settings.at("Window").at("ystart").as<int>(),
            settings.at("Window").at("width").as<int>(),
            settings.at("Window").at("height").as<int>(),
            GW::SYSTEM::GWindowStyle::WINDOWEDBORDERED,
            settings.at("Window").at("title").as<std::string>()});

        auto& window = registry.get<GW::SYSTEM::GWindow>(display);
        auto& input = registry.ctx().emplace<UTIL::Input>();
        input.immediateInput.Create(window);
        input.bufferedInput.Create(window);
        input.gamePads.Create();

        GW::MATH::GMATRIXF cameraMatrix;
        GW::MATH::GMatrix::IdentityF(cameraMatrix);
        const auto camera = registry.create();
        registry.emplace<DRAW::Camera>(camera, DRAW::Camera{cameraMatrix});

        registry.emplace<DRAW::VulkanRendererInitialization>(display,
            DRAW::VulkanRendererInitialization{
                settings.at("Shaders").at("vertex").as<std::string>(),
                settings.at("Shaders").at("pixel").as<std::string>(),
                {{0.04f, 0.06f, 0.09f, 1.0f}},
                {1.0f, 0}, 65.0f, 0.1f, 1000.0f});
        registry.emplace<DRAW::VulkanRenderer>(display);

        auto previous = std::chrono::steady_clock::now();
        while (!registry.any_of<APP::WindowClosed>(display)) {
            const auto now = std::chrono::steady_clock::now();
            registry.ctx().get<UTIL::DeltaTime>().dtSec =
                std::chrono::duration<double>(now - previous).count();
            previous = now;

            GAME::UpdateGameSystems(registry);
            registry.patch<APP::Window>(display);
        }

        // I release input and Vulkan objects before their native window.
        registry.ctx().erase<UTIL::Input>();
        registry.remove<DRAW::VulkanRenderer>(display);
        registry.clear();
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "Startup failed: " << exception.what() << '\n';
        return 1;
    }
}
