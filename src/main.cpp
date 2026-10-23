#include "CCL.h"
#include "DRAW/DrawComponents.h"
#include "APP/Window.hpp"
#include "GAME/GameManager.h"
#include "UTIL/Utilities.h"
#include <windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
WNDPROC gameWindowProc = nullptr;
LRESULT CALLBACK ImGuiWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
    // Keep Gateware's window and gameplay input handling intact.
    return CallWindowProcW(gameWindowProc, hwnd, message, wparam, lparam);
}
void CheckImGuiVulkanResult(VkResult result)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error("ImGui Vulkan operation failed: " + std::to_string(result));
}
}

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
        registry.emplace<DRAW::VulkanRenderer>(display);
        auto &renderer = registry.get<DRAW::VulkanRenderer>(display);
        void *rawInstance = nullptr, *rawQueue = nullptr;
        unsigned int graphicsId = 0, presentId = 0, imageCount = 0;
        GW::SYSTEM::UNIVERSAL_WINDOW_HANDLE windowHandle;
        if (-renderer.vlkSurface.GetInstance(&rawInstance) ||
            -renderer.vlkSurface.GetGraphicsQueue(&rawQueue) ||
            -renderer.vlkSurface.GetQueueFamilyIndices(graphicsId, presentId) ||
            -renderer.vlkSurface.GetSwapchainImageCount(imageCount) ||
            -window.GetWindowHandle(windowHandle) || !rawInstance || !rawQueue ||
            !renderer.physicalDevice || !renderer.device || !renderer.renderPass || imageCount < 2)
            throw std::runtime_error("Vulkan resources unavailable for ImGui initialization");
        const HWND hwnd = static_cast<HWND>(windowHandle.window);
        if (!hwnd)
            throw std::runtime_error("Window unavailable for ImGui initialization");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        if (!ImGui_ImplWin32_Init(hwnd))
            throw std::runtime_error("ImGui Win32 initialization failed");
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.ApiVersion = VK_API_VERSION_1_1; // Gateware's VkApplicationInfo version.
        init_info.Instance = static_cast<VkInstance>(rawInstance);
        init_info.PhysicalDevice = renderer.physicalDevice;
        init_info.Device = renderer.device;
        init_info.QueueFamily = graphicsId;
        init_info.Queue = static_cast<VkQueue>(rawQueue);
        // This backend creates its own SAMPLED_IMAGE + SAMPLER pool with free-set support.
        // Leave the game's uniform/storage pool and allocations unchanged.
        init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
        init_info.CheckVkResultFn = CheckImGuiVulkanResult;
        init_info.MinImageCount = 2;
        init_info.ImageCount = imageCount;
        init_info.PipelineInfoMain.RenderPass = renderer.renderPass;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        if (!ImGui_ImplVulkan_Init(&init_info))
            throw std::runtime_error("ImGui Vulkan initialization failed");
        // Fonts use this backend's dynamic texture upload during RenderDrawData.
        GAME::ButtonStyleSetup();

        SetLastError(0);
        gameWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ImGuiWindowProc)));
        if (!gameWindowProc)
            throw std::runtime_error("Could not forward window messages to ImGui");

        registry.emplace<DRAW::CPULevel>(
            display, DRAW::CPULevel{settings.at("Content").at("level").as<std::string>(),
                                    settings.at("Content").at("models").as<std::string>(),
                                    {}});
        registry.emplace<DRAW::GPULevel>(display);

        GAME::InitializeGameplay(registry);

        auto previous = std::chrono::steady_clock::now();
        while (GAME::CurrentState.Running && !registry.any_of<APP::WindowClosed>(display)) {
            const auto now = std::chrono::steady_clock::now();
            registry.ctx().get<UTIL::DeltaTime>().dtSec =
                std::chrono::duration<double>(now - previous).count();
            previous = now;

            GAME::UpdateGameSystems(registry);
            // Gateware may dispatch window-close events while reading input.
            if (renderer.resourcesReleased)
                break;

            GAME::ProcessLevelTransition(registry);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            GAME::RenderScreens(registry);

            ImGui::Render();

            registry.patch<APP::Window>(display);
        }

        // Wait for UI draws before releasing backend resources and the Vulkan device.
        if (!renderer.resourcesReleased)
            CheckImGuiVulkanResult(vkDeviceWaitIdle(renderer.device));
        if (IsWindow(hwnd))
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gameWindowProc));
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

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
