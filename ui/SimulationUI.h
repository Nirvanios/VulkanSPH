//
// Created by Igor Frank on 30.04.21.
//

#ifndef VULKANAPP_SIMULATIONUI_H
#define VULKANAPP_SIMULATIONUI_H

#include <pf_imgui/elements/Button.h>
#include <pf_imgui/elements/ComboBox.h>
#include <pf_imgui/elements/Group.h>
#include <pf_imgui/elements/Image.h>
#include <pf_imgui/elements/InputText.h>
#include <pf_imgui/elements/Tree.h>
#include "../utils/FPSCounter.h"
#include "../vulkan/enums.h"
#include "ImGuiGlfwVulkan.h"
#include <experimental/memory>

class SimulationUI {
  using ObserverPtrText = std::experimental::observer_ptr<pf::ui::ig::Text>;

 public:
  SimulationUI();
  void init(const std::shared_ptr<Device> &device, const std::shared_ptr<Instance> &instance,
               const vk::RenderPass &renderPass, const vk::UniqueSurfaceKHR &surface,
               const std::shared_ptr<Swapchain> &swapchain, const GlfwWindow &window);
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

 private:
  std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> imgui;
  ObserverPtrText labelFPS;
  ObserverPtrText labelFrameTime;
  ObserverPtrText labelSimStep;
  ObserverPtrText labelYaw;
  ObserverPtrText labelPitch;
  ObserverPtrText labelFramesCount;

  std::function<void(SimulationState)> onButtonSimulationControlClick;
  std::function<void(SimulationState)> onButtonSimulationStepClick;
  std::function<void(SimulationType)> onComboboxSimulationTypeChange;
  std::function<void(RenderType)> onComboboxRenderTypeChange;
  std::function<void(Visualization)> onComboboxVisualizationChange;
  std::function<void(Utilities::Flags<RecordingState>, std::filesystem::path)>
      onButtonRecordingClick;
  std::function<void(Utilities::Flags<RecordingState>)> onButtonScreenshotClick;

  SimulationState simulationState;
  SimulationType selectedSimulationType;
  RenderType selectedRenderType;
  Utilities::Flags<RecordingState> recordingStateFlags;
  Visualization textureVisualization;
  std::function<ImTextureID()> imageProvider;
};

#endif//VULKANAPP_SIMULATIONUI_H
