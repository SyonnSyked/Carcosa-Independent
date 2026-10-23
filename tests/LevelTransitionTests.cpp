#include "precompiled.h"
#include "src/CCL.h"
#include "src/DRAW/DrawComponents.h"
#include "src/APP/Window.hpp"
#include "src/GAME/GameManager.h"
#include "src/GAME/GameplaySystems.h"
#include "src/UTIL/Utilities.h"
#include <stdexcept>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"

namespace {
void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template<typename T> std::size_t Count(entt::registry &r)
{
    const auto view = r.view<T>();
    return std::distance(view.begin(), view.end());
}
void AcquireBossKeyAtExit(entt::registry &r)
{
    const auto player = r.view<GAME::Player>().front();
    const auto boss = r.ctx().get<GAME::DungeonState>().level == 1
        ? r.view<GAME::VampireQueen>().front() : r.view<GAME::UndeadKnight>().front();
    Check(boss != entt::null, "test level has a boss");
    const auto door = r.view<GAME::DungeonExit>().front();
    const auto placement = r.get<GAME::Transform>(door);
    r.get<GAME::Transform>(player) = placement;
    // Preserve the boss's authored scale/rotation: the knight is scaled to 0.01.
    r.get<GAME::Transform>(boss).matrix.row4 = placement.matrix.row4;
    r.get<GAME::Health>(boss).current = 1;
    r.get<GAME::MeleeAttack>(player).remaining = 0;
    GAME::TryMeleeAttack(r, player, boss);
    GAME::HandleEntityDeath(r);
    Check(Count<GAME::DungeonKey>(r) == 1, "boss death drops exactly one key");
    const auto item = r.view<GAME::DungeonKey>().front();
    const auto source = r.ctx().get<DRAW::ModelManager>().models.at("Key").meshEntities.front();
    const auto &expected = r.get<DRAW::GPUInstance>(source).transform;
    const auto &actual = r.get<GAME::Transform>(item).matrix;
    for (int i = 0; i < 12; ++i)
        Check(actual.data[i] == expected.data[i], "dropped key must retain its template scale/orientation");
    Check(actual.row4.x == placement.matrix.row4.x && actual.row4.z == placement.matrix.row4.z,
          "key drops at defeated boss position");
    GAME::CollectDungeonKey(r);
    Check(r.all_of<GAME::DungeonKey>(player), "boss death drops collectible key");
}
void Run()
{
    entt::registry r;
    CCL::InitializeComponentLogic(r);
    auto &settings = *r.ctx().emplace<UTIL::Config>().gameConfig;
    r.ctx().emplace<UTIL::DeltaTime>(UTIL::DeltaTime{0.016});
    const auto display = r.create();
    r.emplace<APP::Window>(display, APP::Window{0, 0, 640, 480,
        GW::SYSTEM::GWindowStyle::WINDOWEDBORDERED, "Carcosa transition regression"});
    auto &window = r.get<GW::SYSTEM::GWindow>(display);
    auto &input = r.ctx().emplace<UTIL::Input>();
    input.immediateInput.Create(window);
    input.bufferedInput.Create(window);
    input.gamePads.Create();
    const auto camera = r.create();
    r.emplace<DRAW::Camera>(camera, DRAW::Camera{GW::MATH::GIdentityMatrixF});
    r.emplace<DRAW::VulkanRendererInitialization>(display,
        DRAW::VulkanRendererInitialization{
            settings.at("Shaders").at("vertex").as<std::string>(),
            settings.at("Shaders").at("pixel").as<std::string>(),
            {{0.04f, 0.06f, 0.09f, 1}}, {1, 0}, 65, 0.1f, 1000});
    r.emplace<DRAW::VulkanRenderer>(display);
    auto &renderer = r.get<DRAW::VulkanRenderer>(display);
    void *instance = nullptr, *queue = nullptr;
    unsigned int graphics = 0, present = 0, images = 0;
    renderer.vlkSurface.GetInstance(&instance);
    renderer.vlkSurface.GetGraphicsQueue(&queue);
    renderer.vlkSurface.GetQueueFamilyIndices(graphics, present);
    renderer.vlkSurface.GetSwapchainImageCount(images);
    GW::SYSTEM::UNIVERSAL_WINDOW_HANDLE handle;
    window.GetWindowHandle(handle);
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // Regression must not write user UI preferences.
    Check(ImGui_ImplWin32_Init(handle.window), "ImGui Win32 init");
    ImGui_ImplVulkan_InitInfo ui{};
    ui.ApiVersion = VK_API_VERSION_1_1;
    ui.Instance = static_cast<VkInstance>(instance);
    ui.PhysicalDevice = renderer.physicalDevice;
    ui.Device = renderer.device;
    ui.QueueFamily = graphics;
    ui.Queue = static_cast<VkQueue>(queue);
    ui.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    ui.MinImageCount = 2;
    ui.ImageCount = images;
    ui.PipelineInfoMain.RenderPass = renderer.renderPass;
    ui.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    Check(ImGui_ImplVulkan_Init(&ui), "ImGui Vulkan init");
    GAME::ButtonStyleSetup();
    const auto uiContext = ImGui::GetCurrentContext();
    const auto uiBackend = ImGui::GetIO().BackendRendererUserData;
    auto draw = [&] {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        GAME::RenderScreens(r);
        ImGui::Render();
        r.patch<APP::Window>(display);
    };
    r.emplace<DRAW::CPULevel>(display, DRAW::CPULevel{
        settings.at("Content").at("level").as<std::string>(),
        settings.at("Content").at("models").as<std::string>(), {}});
    r.emplace<DRAW::GPULevel>(display);
    GAME::InitializeGameplay(r);
    GAME::ChangeScreen(GAME::Screens::Playing);
    const auto device = renderer.device;
    const auto pipeline = renderer.pipeline;
    const auto descriptors = renderer.descriptorSets;
    const auto vertexBuffer = r.get<DRAW::VulkanVertexBuffer>(display).buffer;
    GAME::ChangeScreen(GAME::Screens::MainMenu);
    draw();
    GAME::ChangeScreen(GAME::Screens::Playing);
    draw();

    AcquireBossKeyAtExit(r);
    const auto oldPlayer = r.view<GAME::Player>().front();
    GAME::UseDungeonKey(r);
    GAME::UseDungeonKey(r);
    Check(r.valid(oldPlayer) && !r.all_of<GAME::DungeonKey>(oldPlayer),
          "request consumes key without destroying player");
    Check(r.ctx().get<GAME::DungeonState>().transitionPending &&
          r.ctx().get<GAME::DungeonState>().level == 1, "request defers active level change");
    const auto level2Path = settings.at("Content").at("level2").as<std::string>();
    settings["Content"]["level2"] = "assets/levels/MissingTransitionTestLevel.json";
    GAME::ProcessLevelTransition(r);
    Check(r.ctx().get<GAME::DungeonState>().transitionFailed && r.valid(oldPlayer),
          "missing target rejected before teardown");
    Check(r.get<DRAW::VulkanVertexBuffer>(display).buffer == vertexBuffer &&
          r.all_of<GAME::DungeonKey>(oldPlayer), "rejection keeps buffers and restores key");
    draw();

    // Use the actual Level 2 export so the knight drop is tested with its real scale.
    settings["Content"]["level2"] = level2Path;
    r.ctx().get<GAME::DungeonState>().transitionFailed = false;
    r.get<GAME::Health>(oldPlayer) = GAME::Health{73, 150};
    r.emplace<GAME::BonusStats>(oldPlayer, GAME::BonusStats{12});
    std::vector<entt::entity> oldEntities;
    for (auto [entity] : r.storage<entt::entity>().each())
        if (entity != display && entity != camera)
            oldEntities.push_back(entity);
    GAME::UseDungeonKey(r);
    GAME::ProcessLevelTransition(r);
    Check(r.ctx().get<GAME::DungeonState>().level == 2 &&
          !r.ctx().get<GAME::DungeonState>().transitionPending, "active level is 2");
    for (auto entity : oldEntities)
        Check(!r.valid(entity), "old-level entity survives reload");
    Check(r.valid(camera) && r.all_of<DRAW::Camera>(camera), "camera survives");
    Check(renderer.device == device && renderer.pipeline == pipeline &&
          renderer.descriptorSets == descriptors, "persistent renderer survives");
    Check(ImGui::GetCurrentContext() == uiContext &&
          ImGui::GetIO().BackendRendererUserData == uiBackend, "ImGui context and backend survive");
    Check(Count<GAME::Player>(r) == 1 && Count<GAME::DungeonExit>(r) == 1 &&
          Count<GAME::HordeSpawner>(r) == 1 && Count<GAME::UndeadKnight>(r) == 1 &&
          Count<GAME::VampireQueen>(r) == 0,
          "no duplicate actors, exits or spawners");
    const auto player = r.view<GAME::Player>().front();
    Check(r.get<GAME::Health>(player).current == 73 &&
          r.get<GAME::BonusStats>(player).BonusDamage == 12, "player progression survives");
    Check(!r.all_of<GAME::DungeonKey>(player) &&
          r.ctx().get<GAME::DungeonKeyConfig>().key.level == 2, "new keys match active level");
    Check(r.get<DRAW::VulkanVertexBuffer>(display).buffer != VK_NULL_HANDLE &&
          r.get<DRAW::VulkanIndexBuffer>(display).buffer != VK_NULL_HANDLE, "new buffers uploaded");
    GAME::ProcessLevelTransition(r); // Already consumed; must do nothing.
    for (int frame = 0; frame < 3; ++frame) {
        GAME::UpdateGameSystems(r);
        draw();
    }
    GAME::ChangeScreen(GAME::Screens::PauseMenu);
    draw();
    GAME::ChangeScreen(GAME::Screens::Playing);
    AcquireBossKeyAtExit(r);
    Check(r.get<GAME::DungeonKey>(player).level == 2, "new boss drops Level 2 key");
    GAME::UseDungeonKey(r);
    Check(r.ctx().get<GAME::DungeonState>().complete &&
          GAME::CurrentState.ActiveScreen == GAME::Screens::WinMenu, "Level 2 completes to WinMenu");
    draw();
    GAME::RestartGame(r);
    Check(r.ctx().get<GAME::DungeonState>().level == 2 &&
          Count<GAME::Player>(r) == 1 && Count<GAME::DungeonExit>(r) == 1,
          "restart retains active dungeon without duplicates");
    draw();
    Check(vkDeviceWaitIdle(renderer.device) == VK_SUCCESS, "shutdown GPU wait");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    r.ctx().erase<UTIL::Input>();
    r.remove<DRAW::VulkanRenderer>(display);
    r.clear();
}
}
int main()
{
    try {
        Run();
        Run();
        std::cout << "Level transition regression passed twice with real Vulkan uploads/draws\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Level transition regression failed: " << error.what() << '\n';
        return 1;
    }
}
