//
// Created by Igor Frank on 30.04.21.
//

#ifndef VULKANAPP_SIMULATIONUI_H
#define VULKANAPP_SIMULATIONUI_H

#include "../utils/FPSCounter.h"
#include "../vulkan/enums.h"
#include "ImGuiGlfwVulkan.h"
#include <experimental/memory>
#include <pf_imgui/elements/Button.h>
#include <pf_imgui/elements/ComboBox.h>
#include <pf_imgui/elements/Group.h>
#include <pf_imgui/elements/Image.h>
#include <pf_imgui/elements/InputText.h>
#include <pf_imgui/elements/Tree.h>

class SimulationUI {
  using ObserverPtrText = std::experimental::observer_ptr<pf::ui::ig::Text>;
  using ObserverPtrButton = std::experimental::observer_ptr<pf::ui::ig::Button>;
  using ObserverPtrWindow = std::experimental::observer_ptr<pf::ui::ig::Window>;
  using ObserverPtrTabBar = std::experimental::observer_ptr<pf::ui::ig::TabBar>;

 public:
  SimulationUI();
  void init(const std::shared_ptr<Device> &device, const std::shared_ptr<Instance> &instance,
            const vk::RenderPass &renderPass, const vk::UniqueSurfaceKHR &surface,
            const std::shared_ptr<Swapchain> &swapchain, const GlfwWindow &window);
  void fillSettings(const SimulationInfoSPH &simulationInfoSph,
                    const SimulationInfoGridFluid &simulationInfoGridFluid,
                    const GridInfoMC &gridInfoMc, const FragmentInfo &inFragmentInfo,
                    float initialTempSPH, float coefficientA, float coefficientB);
  void render();
  void addToCommandBuffer(const vk::UniqueCommandBuffer &commandBuffer);
  void onFrameSave(int framesSaved, float recordedSeconds);
  [[nodiscard]] std::function<void(const FPSCounter &, int, float, float)> getFPScallback() const;
  [[nodiscard]] const std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> &getImgui() const;
  [[nodiscard]] bool isHovered();
  [[nodiscard]] bool isKeyboardCaptured();

  void setImageProvider(const std::function<ImTextureID()> &imageProviderCallback);
  void setOnButtonSimulationControlClick(
      const std::function<void(SimulationState)> &onButtonSimulationControlClickCallback);
  void setOnButtonSimulationStepClick(
      const std::function<void(SimulationState)> &onButtonSimulationStepClickCallback);
  void setOnButtonSimulationResetClick(
      const std::function<void(SimulationState)> &onButtonSimulationResetClickCallback);
  void setOnComboboxSimulationTypeChange(
      const std::function<void(SimulationType)> &onComboboxSimulationTypeChangeCallback);
  void setOnComboboxRenderTypeChange(
      const std::function<void(RenderType)> &onComboboxRenderTypeChangeCallback);
  void setOnComboboxVisualizationChange(
      const std::function<void(Visualization)> &onComboboxVisualizationChangeCallback);
  void setOnButtonRecordingClick(
      const std::function<void(Utilities::Flags<RecordingState>, std::filesystem::path)>
          &onButtonRecordingClickCallback);
  void setOnButtonScreenshotClick(
      const std::function<void(Utilities::Flags<RecordingState>)> &onButtonScreenshotClickCallback);
  void setOnFluidColorPicked(const std::function<void(glm::vec4)> &onFluidColorPickedCallback);
  void setOnLightSettingsChanged(
      const std::function<void(FragmentInfo)> &onLightSettingsChangedCallback);
  void setOnMcSettingsChanged(const std::function<void(GridInfoMC)> &onMcSettingsChangedCallback);
  void setOnSettingsSave(const std::function<void(Settings)> &onSettingsSave);
  void setOnButtonSaveState(const std::function<void()> &onButtonSaveStateCallback);
  void setOnButtonLoadState(const std::function<void()> &onButtonLoadStateCallback);

 private:
  std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> imgui;
  ObserverPtrWindow windowMain;
  ObserverPtrWindow windowSettingsSimulation;
  ObserverPtrTabBar tabBarSettingsSimulation;
  ObserverPtrWindow windowSettingsVisualization;
  ObserverPtrText labelFPS;
  ObserverPtrText labelFrameTime;
  ObserverPtrText labelSimStep;
  ObserverPtrText labelYaw;
  ObserverPtrText labelPitch;
  ObserverPtrText labelFramesCount;
  ObserverPtrButton buttonStep;
  ObserverPtrButton buttonReset;
  ObserverPtrButton buttonControl;

  glm::ivec2 windowSize;

  std::function<void(SimulationState)> onButtonSimulationControlClick;
  std::function<void(SimulationState)> onButtonSimulationStepClick;
  std::function<void(SimulationState)> onButtonSimulationResetClick;
  std::function<void(SimulationType)> onComboboxSimulationTypeChange;
  std::function<void(RenderType)> onComboboxRenderTypeChange;
  std::function<void(Visualization)> onComboboxVisualizationChange;
  std::function<void(Utilities::Flags<RecordingState>, std::filesystem::path)>
      onButtonRecordingClick;
  std::function<void(Utilities::Flags<RecordingState>)> onButtonScreenshotClick;
  std::function<void(glm::vec4)> onFluidColorPicked;
  std::function<void(FragmentInfo)> onLightSettingsChanged;
  std::function<void(GridInfoMC)> onMCSettingsChanged;
  std::function<void(Settings)> onSettingsSave;
  std::function<void()> onButtonSaveState;
  std::function<void()> onButtonLoadState;

  Settings settings;
  FragmentInfo fragmentInfo;

  bool rendered = false;

  SimulationState simulationState;
  SimulationType selectedSimulationType;
  RenderType selectedRenderType;
  Utilities::Flags<RecordingState> recordingStateFlags;
  Visualization textureVisualization;
  std::function<ImTextureID()> imageProvider;

  void initSimulationControlGroup(pf::ui::ig::Window &parent);
  void initRecordingGroup(pf::ui::ig::Window &parent);
  void initVisualizationGroup(pf::ui::ig::Window &parent,
                              const std::shared_ptr<Swapchain> &swapchain);
  void initSettingsGroup();
  void initSettingsSimulationSubtree();
  void initSettingsVisualSubtree();
  void initSettingsSimulationSPHSubtree();
  void initSettingsSimulationGridSubtree();
  void initSettingsSimulationEvaporationSubtree();
  void initSettingsSimulationOtherSubtree();
};

#endif//VULKANAPP_SIMULATIONUI_H
