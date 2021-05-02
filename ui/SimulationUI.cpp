//
// Created by Igor Frank on 30.04.21.
//

#include "SimulationUI.h"
SimulationUI::SimulationUI()
    : simulationState(SimulationState::Stopped), selectedSimulationType(SimulationType::SPH),
      selectedRenderType(RenderType::Particles),
      textureVisualization(Visualization::None){
  recordingStateFlags |= RecordingState::Stopped;
}
void SimulationUI::init(const std::shared_ptr<Device> &device,
                        const std::shared_ptr<Instance> &instance, const vk::RenderPass &renderPass,
                        const vk::UniqueSurfaceKHR &surface,
                        const std::shared_ptr<Swapchain> &swapchain, const GlfwWindow &window) {
  using namespace pf::ui;
  imgui = std::make_shared<ig::ImGuiGlfwVulkan>(device, instance, renderPass, surface, swapchain,
                                                window.getWindow().get(), ImGuiConfigFlags{},
                                                toml::table{});

  auto &infoWindow = imgui->createWindow("info_window", "Window");
  labelFPS =
      std::experimental::observer_ptr<ig::Text>(&infoWindow.createChild<ig::Text>("text_FPS", ""));
  labelFrameTime = std::experimental::observer_ptr<ig::Text>(
      &infoWindow.createChild<ig::Text>("text_FrameTime", ""));
  labelSimStep = std::experimental::observer_ptr<ig::Text>(
      &infoWindow.createChild<ig::Text>("text_SimStep", ""));
  labelYaw =
      std::experimental::observer_ptr<ig::Text>(&infoWindow.createChild<ig::Text>("text_yaw", ""));
  labelPitch = std::experimental::observer_ptr<ig::Text>(
      &infoWindow.createChild<ig::Text>("text_pitch:", ""));

  auto &controlTree = infoWindow.createChild<ig::Tree>("control_window", "Simulation control");
  auto &controlGroup = controlTree.createChild<ig::BoxLayout>("box_Controls", ig::LayoutDirection::LeftToRight, ImVec2{infoWindow.getSize().x, 20});

  auto &controlButton =
      controlGroup.createChild<ig::Button>("button_control", "Start simulation");
  auto &buttonReset =
      controlGroup.createChild<ig::Button>("button_reset", "Reset simulation");
  buttonReset.addClickListener([this](){
    onButtonSimulationResetClick(SimulationState::Reset);
  });
  auto &stepButton = controlTree.createChild<ig::Button>("button_step", "Step simulation");
  stepButton.setEnabled(pf::Enabled::Yes);
  controlButton.addClickListener([this, &controlButton, &stepButton, &buttonReset]() {
    if (simulationState == SimulationState::Stopped) {
      controlButton.setLabel("Pause simulation");
      stepButton.setEnabled(pf::Enabled::No);
      buttonReset.setEnabled(pf::Enabled::No);
      simulationState = SimulationState::Simulating;
    } else {
      controlButton.setLabel("Start simulation");
      stepButton.setEnabled(pf::Enabled::Yes);
      buttonReset.setEnabled(pf::Enabled::Yes);
      simulationState = SimulationState::Stopped;
    }
    onButtonSimulationControlClick(simulationState);
  });
  stepButton.addClickListener(
      [this]() { onButtonSimulationStepClick(SimulationState::SingleStep); });

  controlTree
      .createChild<ig::ComboBox<std::string>>(
          "combobox_SimulationSelect", "Simulation:", "SPH",
          [] {
            auto names = magic_enum::enum_names<SimulationType>();
            return std::vector<std::string>{names.begin(), names.end()};
          }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<SimulationType>(value);
        selectedSimulationType = selected.has_value() ? selected.value() : SimulationType::SPH;
        onComboboxSimulationTypeChange(selectedSimulationType);
      });
  controlTree
      .createChild<ig::ComboBox<std::string>>(
          "combobox_RenderSelect", "Render:", "Particles",
          [] {
            auto names = magic_enum::enum_names<RenderType>();
            return std::vector<std::string>{names.begin(), names.end()};
          }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<RenderType>(value);
        selectedRenderType = selected.has_value() ? selected.value() : RenderType::Particles;
        onComboboxRenderTypeChange(selectedRenderType);
      });

  auto &debugVisualizationWindow = imgui->createWindow("window_visualization", "Visualization");
  debugVisualizationWindow
      .createChild<ig::ComboBox<std::string>>(
          "combobox_visualization", "Show", "None",
          [] {
            auto names = magic_enum::enum_names<Visualization>();
            return std::vector<std::string>{names.begin(), names.end()};
          }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<Visualization>(value);
        textureVisualization = selected.has_value() ? selected.value() : Visualization::None;
        onComboboxVisualizationChange(textureVisualization);
      });

  debugVisualizationWindow.createChild<ig::Image>(
      "image_debug", imageProvider(),
      ImVec2{static_cast<float>(swapchain->getExtentWidth() / 4),
             static_cast<float>(swapchain->getExtentHeight() / 4)},
      ig::IsButton::No, [] {
        return std::pair(ImVec2{0, 0}, ImVec2{1, 1});
      });

  auto &recordingWindow = imgui->createWindow("window_Recording", "Recording");
  labelFramesCount =
      std::experimental::observer_ptr<ig::Text>(&recordingWindow.createChild<ig::Text>(
          "text_RecordedFramesCount", "Recorded frames: 0\nRecorded time: 0.0s"));
  auto &editFilename = recordingWindow.createChild<ig::InputText>("memo_Filename", "Filename");

  auto &buttonRecording =
      recordingWindow.createChild<ig::Button>("button_startRecord", "Start Recording");
  buttonRecording.addClickListener([this, &buttonRecording, &editFilename] {
    if (recordingStateFlags.has(RecordingState::Stopped)) {
      buttonRecording.setLabel("Stop Recording");
      recordingStateFlags.clear();
      recordingStateFlags |= RecordingState::Recording;
    } else {
      buttonRecording.setLabel("Start Recording");
      recordingStateFlags.clear();
      recordingStateFlags |= RecordingState::Stopped;
    }
    onButtonRecordingClick(recordingStateFlags, editFilename.getText());
  });
  recordingWindow.createChild<ig::Button>("button_takeScreenshot", "Take Screenshot")
      .addClickListener(
          [this] { onButtonScreenshotClick(recordingStateFlags | RecordingState::Screenshot); });
}
std::function<void(const FPSCounter &, int, float, float)> SimulationUI::getFPScallback() const {
  return [this](auto fpsCounter, int simStep, float yaw, float pitch) {
    labelFPS->setText(fmt::format("FPS: {:.1f} AVG: {:.1f}", fpsCounter.getCurrentFPS(),
                                  fpsCounter.getAverageFPS()));
    labelFrameTime->setText(fmt::format("Frame time: {:.1f}ms, AVG: {:.1f}ms",
                                        fpsCounter.getCurrentFrameTime(),
                                        fpsCounter.getAverageFrameTime()));
    labelSimStep->setText(fmt::format("Simulation step: {}", simStep));
    labelYaw->setText("Yaw: {}", yaw);
    labelPitch->setText("Pitch: {}", pitch);
  };
}

const std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> &SimulationUI::getImgui() const { return imgui; }

void SimulationUI::setImageProvider(const std::function<ImTextureID()> &imageProviderCallback) {
  SimulationUI::imageProvider = imageProviderCallback;
}
void SimulationUI::onFrameSave(int framesSaved, float recordedSeconds) {
  labelFramesCount->setText("Recorded frames: {},\nRecorded time: {:.3}s", framesSaved,
                            recordedSeconds);
}
void SimulationUI::setOnButtonSimulationControlClick(
    const std::function<void(SimulationState)> &onButtonSimulationControlClickCallback) {
  SimulationUI::onButtonSimulationControlClick = onButtonSimulationControlClickCallback;
}
void SimulationUI::setOnButtonSimulationStepClick(
    const std::function<void(SimulationState)> &onButtonSimulationStepClickCallback) {
  SimulationUI::onButtonSimulationStepClick = onButtonSimulationStepClickCallback;
}
void SimulationUI::setOnComboboxSimulationTypeChange(
    const std::function<void(SimulationType)> &onComboboxSimulationTypeChangeCallback) {
  SimulationUI::onComboboxSimulationTypeChange = onComboboxSimulationTypeChangeCallback;
}
void SimulationUI::setOnComboboxRenderTypeChange(
    const std::function<void(RenderType)> &onComboboxRenderTypeChangeCallback) {
  SimulationUI::onComboboxRenderTypeChange = onComboboxRenderTypeChangeCallback;
}
void SimulationUI::setOnComboboxVisualizationChange(
    const std::function<void(Visualization)> &onComboboxVisualizationChangeCallback) {
  SimulationUI::onComboboxVisualizationChange = onComboboxVisualizationChangeCallback;
}
void SimulationUI::setOnButtonRecordingClick(
    const std::function<void(Utilities::Flags<RecordingState>, std::filesystem::path)>
        &onButtonRecordingClickCallback) {
  SimulationUI::onButtonRecordingClick = onButtonRecordingClickCallback;
}
void SimulationUI::setOnButtonScreenshotClick(
    const std::function<void(Utilities::Flags<RecordingState>)> &onButtonScreenshotClickCallback) {
  SimulationUI::onButtonScreenshotClick = onButtonScreenshotClickCallback;
}
void SimulationUI::setOnButtonSimulationResetClick(
    const std::function<void(SimulationState)> &onButtonSimulationResetClickCallback) {
  SimulationUI::onButtonSimulationResetClick = onButtonSimulationResetClickCallback;
}
bool SimulationUI::isHovered() { return imgui->isWindowHovered(); }
bool SimulationUI::isKeyboardCaptured() { return imgui->isKeyboardCaptured(); }
void SimulationUI::render() { imgui->render(); }
void SimulationUI::addToCommandBuffer(const vk::UniqueCommandBuffer &commandBuffer) {
  imgui->addToCommandBuffer(commandBuffer);
}
