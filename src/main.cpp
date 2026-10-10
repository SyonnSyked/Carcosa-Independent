#include "CCL.h"
#include "DRAW/DrawComponents.h"
#include "APP/Window.hpp"
#include "GAME/GameManager.h"
#include "UTIL/Utilities.h"
#include <windows.h>
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"

int main()
{
    try {
        // Resolve resources from the executable, never from the launch directory.
        std::vector<wchar_t> executablePath(1024);
        DWORD length = 0;
        for (;;) {
            length = GetModuleFileNameW(nullptr, executablePath.data(),
                                        static_cast<DWORD>(executablePath.size()));
            if (length == 0)
                throw std::runtime_error("Could not locate the game executable");
            if (length < executablePath.size())
                break;
            executablePath.resize(executablePath.size() * 2);
        }
        const auto executableDirectory =
            std::filesystem::path(std::wstring(executablePath.data(), length)).parent_path();
        const auto resourceRoot = executableDirectory.filename() == L"bin"
                                      ? executableDirectory.parent_path()
                                      : executableDirectory;
        if (!std::filesystem::is_regular_file(resourceRoot / "config/game.ini"))
            throw std::runtime_error("Missing configuration beside the game: " +
                                     resourceRoot.string());
        std::filesystem::current_path(resourceRoot);

        entt::registry registry;
        CCL::InitializeComponentLogic(registry);

        auto &config = registry.ctx().emplace<UTIL::Config>();
        registry.ctx().emplace<UTIL::DeltaTime>();
        const auto &settings = *config.gameConfig;

        const auto display = registry.create();
        registry.emplace<APP::Window>(
            display, APP::Window{settings.at("Window").at("xstart").as<int>(),
                                 settings.at("Window").at("ystart").as<int>(),
                                 settings.at("Window").at("width").as<int>(),
                                 settings.at("Window").at("height").as<int>(),
                                 GW::SYSTEM::GWindowStyle::WINDOWEDBORDERED,
                                 settings.at("Window").at("title").as<std::string>()});

        auto &window = registry.get<GW::SYSTEM::GWindow>(display);
        auto &input = registry.ctx().emplace<UTIL::Input>();
        input.immediateInput.Create(window);
        input.bufferedInput.Create(window);
        input.gamePads.Create();

        GW::MATH::GMATRIXF cameraMatrix;
        GW::MATH::GMatrix::IdentityF(cameraMatrix);
        cameraMatrix.row4.x = settings.at("PreviewCamera").at("x").as<float>();
        cameraMatrix.row4.y = settings.at("PreviewCamera").at("y").as<float>();
        cameraMatrix.row4.z = settings.at("PreviewCamera").at("z").as<float>();
        const auto camera = registry.create();
        registry.emplace<DRAW::Camera>(camera, DRAW::Camera{cameraMatrix});

        registry.emplace<DRAW::VulkanRendererInitialization>(display,
            DRAW::VulkanRendererInitialization{
                settings.at("Shaders").at("vertex").as<std::string>(),
                settings.at("Shaders").at("pixel").as<std::string>(),
                {{0.04f, 0.06f, 0.09f, 1.0f}},
                {1.0f, 0}, 65.0f, 0.1f, 1000.0f});
        // bro idk wtf is this dog shit unity better
            auto& renderer = registry.get<DRAW::VulkanRenderer>(display);
             
            void* rawInstance = nullptr;
            renderer.vlkSurface.GetInstance(&rawInstance);

            void* rawQueue = nullptr;
            renderer.vlkSurface.GetGraphicsQueue(&rawQueue);

            unsigned int graphicsId = 0,PresentId =0;
            renderer.vlkSurface.GetQueueFamilyIndices(&graphicsId,&PresentId);

            GW::SYSTEM::UNIVERSAL_WINDOW_HANDLE windowHand;
            window.GetWindowHandle(windowHand);
            HWND hwnd = static_cast<HWND>(windowHand.window);

            ImGui::CreateContext();
            ImGui_ImplWin32_Init(hwnd);

            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = static_cast<VkInstance>(rawInstance);
            init_info.PhysicalDevice = renderer.physicalDevice;
            init_info.Device = renderer.device;
            init_info.QueueFamily = graphicsIdx;
            init_info.Queue = static_cast<VkQueue>(rawQueue);
            init_info.DescriptorPool = renderer.descriptorPool;
            init_info.MinImageCount = 2;
            init_info.ImageCount = 2;
            init_info.PipelineInfoMain.RenderPass = renderer.renderPass;
            init_info.PipelineInfoMain.Subpass = 0;
            init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
       
            ImGui_ImplVulkan_Init(&init_info);
            ButtonStyleSetup();
        // end of the shit i tried to do

        registry.emplace<DRAW::VulkanRendererInitialization>(
            display, DRAW::VulkanRendererInitialization{
                         settings.at("Shaders").at("vertex").as<std::string>(),
                         settings.at("Shaders").at("pixel").as<std::string>(),
                         {{0.04f, 0.06f, 0.09f, 1.0f}},
                         {1.0f, 0},
                         65.0f,
                         0.1f,
                         1000.0f});
        registry.emplace<DRAW::VulkanRenderer>(display);

        registry.emplace<DRAW::CPULevel>(
            display, DRAW::CPULevel{settings.at("Content").at("level").as<std::string>(),
                                    settings.at("Content").at("models").as<std::string>(),
                                    {}});
        registry.emplace<DRAW::GPULevel>(display);

        GAME::InitializeGameplay(registry);

        auto previous = std::chrono::steady_clock::now();
        while (!registry.any_of<APP::WindowClosed>(display)) {
            const auto now = std::chrono::steady_clock::now();
            registry.ctx().get<UTIL::DeltaTime>().dtSec =
                std::chrono::duration<double>(now - previous).count();
            previous = now;

            GAME::UpdateGameSystems(registry);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            GAME::RenderScreens(registry);

            ImGui::Render();

            registry.patch<APP::Window>(display);
        }

        // I release input and Vulkan objects before their native window.
        registry.ctx().erase<UTIL::Input>();
        registry.remove<DRAW::VulkanRenderer>(display);
        registry.clear();
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "Startup failed: " << exception.what() << '\n';
        return 1;
    }
}
