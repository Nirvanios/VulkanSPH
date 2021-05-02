//
// Created by Igor Frank on 30.04.21.
//

#include "SimulationUI.h"
#include "pf_imgui/elements/ColorChooser.h"
#include "pf_imgui/elements/DragInput.h"
#include <pf_imgui/elements/Input.h>
#include <pf_imgui/elements/Slider.h>
#include <pf_imgui/elements/Slider3D.h>
SimulationUI::SimulationUI()
    : simulationState(SimulationState::Stopped), selectedSimulationType(SimulationType::SPH),
      selectedRenderType(RenderType::Particles), textureVisualization(Visualization::None) {
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
  /**Info window*/
  windowMain = std::experimental::observer_ptr<ig::Window>(
      &imgui->createWindow("window_main", "Simulation"));
  windowMain->setCollapsible(true);
  auto &infoGroup = windowMain->createChild<ig::Group>("group_info", "Info");
  labelFPS =
      std::experimental::observer_ptr<ig::Text>(&infoGroup.createChild<ig::Text>("text_FPS", ""));
  labelFrameTime = std::experimental::observer_ptr<ig::Text>(
      &infoGroup.createChild<ig::Text>("text_FrameTime", ""));
  labelSimStep = std::experimental::observer_ptr<ig::Text>(
      &infoGroup.createChild<ig::Text>("text_SimStep", ""));
  labelYaw =
      std::experimental::observer_ptr<ig::Text>(&infoGroup.createChild<ig::Text>("text_yaw", ""));
  labelPitch = std::experimental::observer_ptr<ig::Text>(
      &infoGroup.createChild<ig::Text>("text_pitch:", ""));

  initSimulationControlGroup(*windowMain);

  initRecordingGroup(*windowMain);

  initSettingsGroup(*windowMain);

  initVisualizationGroup(*windowMain, swapchain);
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
void SimulationUI::setOnColorPicked(const std::function<void(glm::vec4)> &onColorPickedCallback) {
  SimulationUI::onColorPicked = onColorPickedCallback;
}
void SimulationUI::initSettingsGroup(pf::ui::ig::Window &parent) {
  using namespace pf::ui;

  auto &treeSettings =
      parent.createChild<ig::Group>("tree_settings", "Settings", ig::Persistent::Yes, ig::AllowCollapse::Yes);
  initSettingsVisualSubtree(treeSettings);
  initSettingsSimulationSubtree(treeSettings);
}
void SimulationUI::initSimulationControlGroup(pf::ui::ig::Window &parent) {
  using namespace pf::ui;
  /**Simulation control*/
  auto &treeControl = parent.createChild<ig::Group>("tree_control", "Simulation control",
                                                    ig::Persistent::Yes, ig::AllowCollapse::Yes);
  auto &controlGroup = treeControl.createChild<ig::BoxLayout>(
      "box_Controls", ig::LayoutDirection::LeftToRight, ImVec2{250, 20});

  auto &controlButton = controlGroup.createChild<ig::Button>("button_control", "Start simulation");
  auto &buttonReset = controlGroup.createChild<ig::Button>("button_reset", "Reset simulation");
  buttonReset.addClickListener([this]() { onButtonSimulationResetClick(SimulationState::Reset); });
  auto &stepButton = treeControl.createChild<ig::Button>("button_step", "Step simulation");
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

  treeControl
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
  treeControl
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
}
void SimulationUI::initSettingsVisualSubtree(pf::ui::ig::Group &parent) {
  using namespace pf::ui;
  //auto lightPosLimits = glm::vec2{-20, 20};

  auto &treeSettingsRender = parent.createChild<ig::Tree>("tree_settingsVisual", "Visual Settings");
  /**Light*/
/*  auto &treeLightPos =
      treeSettingsRender.createChild<ig::Tree>("tree_lightPosition", "Light position");*/
/*  [[maybe_unused]] auto &positionLight = treeLightPos.createChild<ig::Slider3D<float>>(
      "slider3d_lightPos", "", lightPosLimits, lightPosLimits, lightPosLimits, glm::vec3{0, 5, 0}, 0.5);*/
  auto &treeLightColor = treeSettingsRender.createChild<ig::Tree>("tree_lightColor", "Light color");
  auto &lightColor =
      treeLightColor.createChild<ig::ColorChooser<ig::ColorChooserType::Edit, glm::vec4>>(
          "color_Fluid", "");
  lightColor.setValue(glm::vec4{1.0});

  /**MC*/
  auto &treeMC = treeSettingsRender.createChild<ig::Tree>("tree_MC", "Marching cubes");
  treeMC.createChild<ig::Slider<int>>("input_detailMC", "Deatil", 1, 5, 1, ig::Persistent::No);
  treeMC.createChild<ig::Slider<float>>("input_threshold", "SDF Threshold", 0.01, 1, 0.5,
                                        ig::Persistent::No);
  /**Color picker*/
  auto &treeColor = treeSettingsRender.createChild<ig::Tree>("tree_colorPick", "Fluid color");
  auto &colorPicker =
      treeColor.createChild<ig::ColorChooser<ig::ColorChooserType::Edit, glm::vec4>>(
          "color_Fluid", "Fluid color");
  colorPicker.setValue(glm::vec4{0.5, 0.8, 1.0, 1.0});
  colorPicker.addValueListener(
      [this, &colorPicker](auto) { onColorPicked(colorPicker.getValue()); });
}
void SimulationUI::initSettingsSimulationSubtree(pf::ui::ig::Group &parent) {
  using namespace pf::ui;

  auto &treeSettingsSimulation =
      parent.createChild<ig::Tree>("tree_settingsSimulation", "Simulation Settings");

  initSettingsSimulationSPHSubtree(treeSettingsSimulation);
  initSettingsSimulationGridSubtree(treeSettingsSimulation);
  //auto &treeSettingsSimulationEvaporation = treeSettingsSimulation.createChild<ig::Tree>("tree_settingsSimulationEvaporation", "Evaporation");
  initSettingsSimulationOtherSubtree(treeSettingsSimulation);

  auto &groupSettingsButtons = treeSettingsSimulation.createChild<ig::BoxLayout>(
      "box_settingsButtons", ig::LayoutDirection::LeftToRight, ImVec2{350, 20});
  auto &buttonSettingsSave = groupSettingsButtons.createChild<ig::Button>(
      "button_saveSettings", "Save and Reset Simulation");
  buttonSettingsSave.addClickListener([] {});
  auto &buttonSettingsReset =
      groupSettingsButtons.createChild<ig::Button>("button_resetSettings", "Reset values");
  buttonSettingsReset.addClickListener([] {});


}
void SimulationUI::initSettingsSimulationSPHSubtree(pf::ui::ig::Tree &parent) {
  using namespace pf::ui;

  auto &treeSettingsSimulationSPH =
      parent.createChild<ig::Tree>("tree_settingsSimulationSPH", "SPH");

  /***/
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_volume", "Volume", 0.1, 1.0, 2.0,
                                                          ig::Persistent::No);
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_density", "Rest density", 0, 0,
                                                          0, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_viscosity", "Viscosity", 0, 0,
                                                          0, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_gasStiff", "Gas stiffnes", 0, 0,
                                                          0, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_HeatConSPH", "Heat conductivity", 0, 0,
                                                          0, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_HeatCapSPH", "Heat capacity", 0, 0,
                                                          0, ig::Persistent::Yes);
/*  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_TenThre", "Tension threshold", 0, 0,
                                                          0, ig::Persistent::Yes);*/
  treeSettingsSimulationSPH.createChild<ig::Input<float>>("input_TenCoeff", "Tension coefficient", 0, 0,
                                                          0, ig::Persistent::Yes);

  treeSettingsSimulationSPH.createChild<ig::Text>("group_fluidSize", "Initial fluid size");
  treeSettingsSimulationSPH.createChild<ig::Input<int>>("input_fluidSizeX", "x", 1, 5, 20, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Input<int>>("input_fluidSizeY", "y", 1, 5, 20, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Input<int>>("input_fluidSizeZ", "z", 1, 5, 20, ig::Persistent::Yes);
  treeSettingsSimulationSPH.createChild<ig::Text>("text_particleCount", "Particle count ");

}
void SimulationUI::initRecordingGroup(pf::ui::ig::Window &parent) {
  using namespace pf::ui;

  /**Recording*/
  auto &recordingWindow = parent.createChild<ig::Group>("tree_Recording", "Recording", ig::Persistent::Yes, ig::AllowCollapse::Yes);
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
void SimulationUI::initVisualizationGroup(pf::ui::ig::Window &parent,
                                         const std::shared_ptr<Swapchain> &swapchain) {
  using namespace pf::ui;

  /**Visualization*/
  auto &debugVisualizationWindow =
      parent.createChild<ig::Group>("tree_visualization", "Visualization", ig::Persistent::Yes, ig::AllowCollapse::Yes);
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
}
void SimulationUI::initSettingsSimulationOtherSubtree(pf::ui::ig::Tree &parent) {
  using namespace pf::ui;

  auto &treeSettingsOtherSimulation =
      parent.createChild<ig::Tree>("tree_settingsSimulation", "Other");

  treeSettingsOtherSimulation.createChild<ig::Input<float>>("input_timestep", "Timestep", 0.0001, 0.1,
      0.001, ig::Persistent::No, "%.4f");

  treeSettingsOtherSimulation.createChild<ig::Text>("label_gridSize", "Grid size");

  treeSettingsOtherSimulation.createChild<ig::Input<int>>("input_gridSizeX", "x", 1, 5, 20, ig::Persistent::No);
  treeSettingsOtherSimulation.createChild<ig::Input<int>>("input_gridSizeY", "y", 1, 5, 20, ig::Persistent::No);
  treeSettingsOtherSimulation.createChild<ig::Input<int>>("input_gridSizeZ", "z", 1, 5, 20, ig::Persistent::No);
}
void SimulationUI::initSettingsSimulationGridSubtree(pf::ui::ig::Tree &parent) {
  using namespace pf::ui;

  auto &treeSettingsSimulationGrid =
      parent.createChild<ig::Tree>("tree_settingsSimulationGrid", "Euler grid");

  treeSettingsSimulationGrid.createChild<ig::Input<float>>("input_diff", "Diffusion coefficient", 0, 0,
      0, ig::Persistent::Yes);
  treeSettingsSimulationGrid.createChild<ig::Input<float>>("input_gasConst", "Specific gas constant", 0, 0,
      0, ig::Persistent::Yes);
  treeSettingsSimulationGrid.createChild<ig::Input<float>>("input_HeatConGrid", "Heat conductivity", 0, 0,
      0, ig::Persistent::Yes);
  treeSettingsSimulationGrid.createChild<ig::Input<float>>("input_HeatCapGrid", "Heat capacity", 0, 0,
      0, ig::Persistent::Yes);
}
