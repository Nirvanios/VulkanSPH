//
// Created by Igor Frank on 30.04.21.
//

#include "SimulationUI.h"
#include "glm/gtx/component_wise.hpp"
#include "pf_imgui/elements/ColorChooser.h"
#include "pf_imgui/elements/DragInput.h"
#include <pf_imgui/elements/Input.h>
#include <pf_imgui/elements/Slider.h>
#include <pf_imgui/elements/Slider3D.h>
#include <pf_imgui/elements/TabBar.h>
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
  windowMain->setPosition(ImVec2{5, 5});
  windowMain->setSize(ImVec2{static_cast<float>(swapchain->getExtentWidth() - 10),
                             static_cast<float>(swapchain->getExtentHeight() - 10)});
  auto &menuBar = windowMain->getMenuBar();
  auto &barFile = menuBar.addSubmenu("bar_File", "File");
  barFile.addItem("item_FileExit", "Exit").addClickListener([&] { window.setCloseFlag(); });

  auto &barSettings = menuBar.addSubmenu("bar_File", "Settings");
  barSettings.addItem("item_FileExit", "Simulation settings").addClickListener([this] {
    windowSettingsSimulation->setVisibility(ig::Visibility::Visible);
  });
  barSettings.addItem("item_FileExit", "Visual settings").addClickListener([this] {
    windowSettingsVisualization->setVisibility(ig::Visibility::Visible);
  });

  windowMain->setCollapsible(true);
  auto &infoGroup = windowMain->createChild<ig::Group>("group_info", "Info");
  auto &infoBox = infoGroup.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 100});
  infoBox.setDrawBorder(true);
  labelFPS =
      std::experimental::observer_ptr<ig::Text>(&infoBox.createChild<ig::Text>("text_FPS", ""));
  labelFrameTime = std::experimental::observer_ptr<ig::Text>(
      &infoBox.createChild<ig::Text>("text_FrameTime", ""));
  labelSimStep =
      std::experimental::observer_ptr<ig::Text>(&infoBox.createChild<ig::Text>("text_SimStep", ""));
  labelYaw =
      std::experimental::observer_ptr<ig::Text>(&infoBox.createChild<ig::Text>("text_yaw", ""));
  labelPitch =
      std::experimental::observer_ptr<ig::Text>(&infoBox.createChild<ig::Text>("text_pitch:", ""));

  initSimulationControlGroup(*windowMain);

  initRecordingGroup(*windowMain);

  initSettingsGroup();

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
void SimulationUI::setOnFluidColorPicked(
    const std::function<void(glm::vec4)> &onFluidColorPickedCallback) {
  SimulationUI::onFluidColorPicked = onFluidColorPickedCallback;
}
void SimulationUI::initSettingsGroup() {
  using namespace pf::ui;

  initSettingsVisualSubtree();
  initSettingsSimulationSubtree();
}
void SimulationUI::initSimulationControlGroup(pf::ui::ig::Window &parent) {
  using namespace pf::ui;
  /**Simulation control*/
  auto &groupControl = parent.createChild<ig::Group>("tree_control", "Simulation control",
                                                     ig::Persistent::Yes, ig::AllowCollapse::Yes);
  auto &controlBox = groupControl.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 130});
  controlBox.setDrawBorder(true);

  groupControl.setCollapsed(true);
  auto &groupControlButtons = controlBox.createChild<ig::BoxLayout>(
      "box_Controls", ig::LayoutDirection::LeftToRight, ImVec2{250, 20});

  buttonControl = std::experimental::make_observer(
      &groupControlButtons.createChild<ig::Button>("button_control", "Start simulation"));
  buttonReset = std::experimental::make_observer(
      &groupControlButtons.createChild<ig::Button>("button_reset", "Reset simulation"));
  buttonReset->addClickListener([this]() { onButtonSimulationResetClick(SimulationState::Reset); });
  buttonStep = std::experimental::make_observer(
      &controlBox.createChild<ig::Button>("button_step", "Step simulation"));
  buttonStep->setEnabled(pf::Enabled::Yes);
  buttonControl->addClickListener([this]() {
    if (simulationState == SimulationState::Stopped) {
      buttonControl->setLabel("Pause simulation");
      buttonStep->setEnabled(pf::Enabled::No);
      buttonReset->setEnabled(pf::Enabled::No);
      simulationState = SimulationState::Simulating;
    } else {
      buttonControl->setLabel("Start simulation");
      buttonStep->setEnabled(pf::Enabled::Yes);
      buttonReset->setEnabled(pf::Enabled::Yes);
      simulationState = SimulationState::Stopped;
    }
    onButtonSimulationControlClick(simulationState);
  });
  buttonStep->addClickListener(
      [this]() { onButtonSimulationStepClick(SimulationState::SingleStep); });

  controlBox
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
  controlBox
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

  auto &buttonBox = controlBox.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::LeftToRight, ImVec2{-1, 20});

  buttonBox.createChild<ig::Button>("button_saveState", "Save State").addClickListener([this] {
    onButtonSaveState();
  });
  buttonBox.createChild<ig::Button>("button_loadState", "Load State").addClickListener([this] {
    onButtonLoadState();
  });
}
void SimulationUI::initSettingsVisualSubtree() {
  using namespace pf::ui;
  auto lightPosLimits = glm::vec2{-50, 50};

  windowSettingsVisualization = std::experimental::make_observer(
      &imgui->createWindow("window_visualSettings", "Visualization Settings"));
  windowSettingsVisualization->setVisibility(ig::Visibility::Invisible);

  auto &tabBarVisual = windowSettingsVisualization->createChild<ig::TabBar>("tabBar_visual");
  auto &tabLight = tabBarVisual.addTab("tab_visualLight", "Light");
  auto &tabOther = tabBarVisual.addTab("tab_visualOther", "Other");

  /**Light*/
  auto &boxLight = tabLight.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 260});
  boxLight.setDrawBorder(true);

  boxLight.createChild<ig::Text>(ig::uniqueId(), "Light position");
  boxLight
      .createChild<ig::Slider3D<float>>("slider3d_lightPos", "", lightPosLimits, lightPosLimits,
                                        lightPosLimits, fragmentInfo.lightPosition.xyz(), 0.5)
      .addValueListener([this](auto value) {
        fragmentInfo.lightPosition = glm::vec4{value, 0.0};
        onLightSettingsChanged(fragmentInfo);
      });
  auto &lightColor = boxLight.createChild<ig::ColorChooser<ig::ColorChooserType::Edit, glm::vec4>>(
      "color_Fluid", "Light color");
  lightColor.setValue(fragmentInfo.lightColor);
  lightColor.addValueListener([this](auto value) {
    fragmentInfo.lightColor = value;
    onLightSettingsChanged(fragmentInfo);
  });

  /**Color picker*/
  auto &boxFluid = tabOther.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 50});
  boxFluid.setDrawBorder(true);

  boxFluid.createChild<ig::Text>(ig::uniqueId(), "Fluid");

  auto &colorPicker = boxFluid.createChild<ig::ColorChooser<ig::ColorChooserType::Edit, glm::vec4>>(
      "color_Fluid", "Color");
  colorPicker.setValue(glm::vec4{0.5, 0.8, 1.0, 1.0});
  colorPicker.addValueListener(
      [this, &colorPicker](auto) { onFluidColorPicked(colorPicker.getValue()); });

  auto &boxMC = tabOther.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 70});
  boxMC.setDrawBorder(true);

  boxMC.createChild<ig::Text>(ig::uniqueId(), "Marching cubes");

  /**MC*/
  //auto &treeMC = windowSettingsVisualization->createChild<ig::Tree>("tree_MC", "Marching cubes");
  boxMC
      .createChild<ig::Slider<int>>("input_detailMC", "Deatil", 1, 5, settings.gridInfoMC.detail,
                                    ig::Persistent::Yes)
      .addValueListener([this](auto value) {
        settings.gridInfoMC.detail = value;
        settings.gridInfoMC.gridSize = value * (settings.simulationInfoGridFluid.gridSize + 2);
        settings.gridInfoMC.cellSize =
            settings.simulationInfoGridFluid.cellSize / static_cast<float>(value);
        onMCSettingsChanged(settings.gridInfoMC);
      });
  boxMC
      .createChild<ig::Slider<float>>("input_threshold", "SDF Threshold", 0.01, 1, 0.5,
                                      ig::Persistent::No)
      .addValueListener([this](auto value) {
        settings.gridInfoMC.threshold = value;
        onMCSettingsChanged(settings.gridInfoMC);
      });

  windowSettingsVisualization->createChild<ig::Button>("button_visualSettingClose", "Close")
      .addClickListener(
          [this] { windowSettingsVisualization->setVisibility(ig::Visibility::Invisible); });
}
void SimulationUI::initSettingsSimulationSubtree() {
  using namespace pf::ui;

  /*  auto &treeSettingsSimulation =
      parent.createChild<ig::Tree>("tree_settingsSimulation", "Simulation Settings");*/

  windowSettingsSimulation = std::experimental::make_observer(
      &imgui->createWindow("windowSettingsSimulation", "Settings"));
  windowSettingsSimulation->setVisibility(ig::Visibility::Invisible);
  tabBarSettingsSimulation = std::experimental::make_observer(
      &windowSettingsSimulation->createChild<ig::TabBar>("tabBar_Settings"));

  initSettingsSimulationSPHSubtree();
  initSettingsSimulationGridSubtree();
  initSettingsSimulationEvaporationSubtree();
  initSettingsSimulationOtherSubtree();

  auto &groupSettingsButtons = windowSettingsSimulation->createChild<ig::BoxLayout>(
      "box_settingsButtons", ig::LayoutDirection::LeftToRight, ImVec2{350, 20});
  auto &buttonSettingsSave = groupSettingsButtons.createChild<ig::Button>(
      "button_saveSettings", "Save and Reset Simulation");
  buttonSettingsSave.addClickListener([this] {
    buttonControl->setLabel("Start simulation");
    buttonStep->setEnabled(pf::Enabled::Yes);
    buttonReset->setEnabled(pf::Enabled::Yes);
    simulationState = SimulationState::Stopped;
    onSettingsSave(settings);
  });
  auto &buttonSettingsReset =
      groupSettingsButtons.createChild<ig::Button>("button_closeSettings", "Close");
  buttonSettingsReset.addClickListener(
      [this] { windowSettingsSimulation->setVisibility(ig::Visibility::Invisible); });
}
void SimulationUI::initSettingsSimulationSPHSubtree() {
  using namespace pf::ui;

  auto &tabSettingsSimulationSPH =
      tabBarSettingsSimulation->addTab("tree_settingsSimulationSPH", "SPH");

  auto &boxSPH = tabSettingsSimulationSPH.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 170});
  boxSPH.setDrawBorder(true);

  /***/
  boxSPH
      .createChild<ig::Input<float>>("input_density", "Rest density", 0, 0,
                                     settings.simulationInfoSPH.restDensity, ig::Persistent::Yes)
      .addValueListener([this](auto value) { settings.simulationInfoSPH.restDensity = value; });
  boxSPH
      .createChild<ig::Input<float>>("input_viscosity", "Viscosity", 0, 0,
                                     settings.simulationInfoSPH.viscosityCoefficient,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoSPH.viscosityCoefficient = value; });

  boxSPH
      .createChild<ig::Input<float>>("input_temp", "Temperature", 0, 0,
                                     settings.initialSPHTemperature, ig::Persistent::Yes)
      .addValueListener([this](auto value) { settings.initialSPHTemperature = value; });
  boxSPH
      .createChild<ig::Input<float>>("input_gasStiff", "Gas stiffnes", 0, 0,
                                     settings.simulationInfoSPH.gasStiffnessConstant,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoSPH.gasStiffnessConstant = value; });
  boxSPH
      .createChild<ig::Input<float>>("input_HeatConSPH", "Heat conductivity", 0, 0,
                                     settings.simulationInfoSPH.heatConductivity,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoSPH.heatConductivity = value; });
  boxSPH
      .createChild<ig::Input<float>>("input_HeatCapSPH", "Heat capacity", 0, 0,
                                     settings.simulationInfoSPH.heatCapacity, ig::Persistent::Yes)
      .addValueListener([this](auto value) { settings.simulationInfoSPH.heatCapacity = value; });

  boxSPH
      .createChild<ig::Input<float>>("input_TenCoeff", "Tension coefficient", 0, 0,
                                     settings.simulationInfoSPH.tensionCoefficient,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoSPH.tensionCoefficient = value; });
}
void SimulationUI::initRecordingGroup(pf::ui::ig::Window &parent) {
  using namespace pf::ui;

  /**Recording*/
  auto &groupRecording = parent.createChild<ig::Group>("tree_Recording", "Recording",
                                                       ig::Persistent::Yes, ig::AllowCollapse::Yes);
  groupRecording.setCollapsed(true);

  auto &boxRecording = groupRecording.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 90});
  boxRecording.setDrawBorder(true);

  labelFramesCount = std::experimental::observer_ptr<ig::Text>(&boxRecording.createChild<ig::Text>(
      "text_RecordedFramesCount", "Recorded frames: 0\nRecorded time: 0.0s"));
  auto &editFilename = boxRecording.createChild<ig::InputText>("memo_Filename", "Filename");

  auto &buttonBox = boxRecording.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::LeftToRight, ImVec2{-1, 20});

  auto &buttonRecording =
      buttonBox.createChild<ig::Button>("button_startRecord", "Start Recording");
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
  buttonBox.createChild<ig::Button>("button_takeScreenshot", "Take Screenshot")
      .addClickListener(
          [this] { onButtonScreenshotClick(recordingStateFlags | RecordingState::Screenshot); });
}
void SimulationUI::initVisualizationGroup(pf::ui::ig::Window &parent,
                                          const std::shared_ptr<Swapchain> &swapchain) {
  using namespace pf::ui;

  /**Visualization*/
  auto &groupVisualization = parent.createChild<ig::Group>(
      "tree_visualization", "Visualization", ig::Persistent::Yes, ig::AllowCollapse::Yes);
  groupVisualization.setCollapsed(true);

  auto &boxVisualization = groupVisualization.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 220});
  boxVisualization.setDrawBorder(true);

  boxVisualization
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

  boxVisualization.createChild<ig::Image>(
      "image_debug", imageProvider(),
      ImVec2{static_cast<float>(swapchain->getExtentWidth() / 4),
             static_cast<float>(swapchain->getExtentHeight() / 4)},
      ig::IsButton::No, [] {
        return std::pair(ImVec2{0, 0}, ImVec2{1, 1});
      });
}
void SimulationUI::initSettingsSimulationOtherSubtree() {
  using namespace pf::ui;

  auto &tabSettingsOtherSimulation =
      tabBarSettingsSimulation->addTab("tree_settingsSimulation", "Other");

  auto &boxOther = tabSettingsOtherSimulation.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 170});
  boxOther.setDrawBorder(true);

  boxOther
      .createChild<ig::Input<float>>("input_timestep", "Timestep", 0.0001, 0.1,
                                     settings.simulationInfoSPH.timeStep, ig::Persistent::No,
                                     "%.4f")
      .addValueListener([this](auto value) { settings.simulationInfoSPH.timeStep = value; });
}
void SimulationUI::initSettingsSimulationGridSubtree() {
  using namespace pf::ui;

  auto &tabSettingsSimulationGrid =
      tabBarSettingsSimulation->addTab("tree_settingsSimulationGrid", "Euler grid");

  auto &boxGrid = tabSettingsSimulationGrid.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 170});
  boxGrid.setDrawBorder(true);

  boxGrid
      .createChild<ig::Input<float>>("input_ambient", "Ambient temperature", 0, 0,
                                     settings.simulationInfoGridFluid.ambientTemperature,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.ambientTemperature = value; });
  boxGrid
      .createChild<ig::Input<float>>("input_bouyancyA", "Bouyancy alpha", 0, 0,
                                     settings.simulationInfoGridFluid.buoyancyAlpha,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.buoyancyAlpha = value; });
  boxGrid
      .createChild<ig::Input<float>>("input_bouyancyB", "Bouyancy beta", 0, 0,
                                     settings.simulationInfoGridFluid.buoyancyBeta,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.buoyancyBeta = value; });

  boxGrid
      .createChild<ig::Input<float>>("input_diff", "Diffusion coefficient", 0, 0,
                                     settings.simulationInfoGridFluid.diffusionCoefficient,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.diffusionCoefficient = value; });

  boxGrid
      .createChild<ig::Input<float>>("input_gasConst", "Specific gas constant", 0, 0,
                                     settings.simulationInfoGridFluid.specificGasConstant,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.specificGasConstant = value; });

  boxGrid
      .createChild<ig::Input<float>>("input_HeatConGrid", "Heat conductivity", 0, 0,
                                     settings.simulationInfoGridFluid.heatConductivity,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.heatConductivity = value; });

  boxGrid
      .createChild<ig::Input<float>>("input_HeatCapGrid", "Heat capacity", 0, 0,
                                     settings.simulationInfoGridFluid.heatCapacity,
                                     ig::Persistent::Yes)
      .addValueListener(
          [this](auto value) { settings.simulationInfoGridFluid.heatCapacity = value; });
}
void SimulationUI::initSettingsSimulationEvaporationSubtree() {
  using namespace pf::ui;

  auto &tabSettingsSimulationEvaporation =
      tabBarSettingsSimulation->addTab("tree_settingsSimulationEvaporation", "Evaporation");

  auto &boxEvaporation = tabSettingsSimulationEvaporation.createChild<ig::BoxLayout>(
      ig::uniqueId(), ig::LayoutDirection::TopToBottom, ImVec2{-1, 170});
  boxEvaporation.setDrawBorder(true);

  boxEvaporation
      .createChild<ig::Input<float>>("input_coeffA", "A (rate)", 0, 0, settings.coefficientA,
                                     ig::Persistent::Yes)
      .addValueListener([this](auto value) { settings.coefficientA = value; });
  boxEvaporation
      .createChild<ig::Input<float>>("input_coeffB", "B (rate velocity)", 0, 0,
                                     settings.coefficientB, ig::Persistent::Yes)
      .addValueListener([this](auto value) { settings.coefficientB = value; });
}
void SimulationUI::setOnSettingsSave(const std::function<void(Settings)> &onSettingsSaveCallback) {
  SimulationUI::onSettingsSave = onSettingsSaveCallback;
}
void SimulationUI::fillSettings(const SimulationInfoSPH &simulationInfoSph,
                                const SimulationInfoGridFluid &simulationInfoGridFluid,
                                const GridInfoMC &gridInfoMc, const FragmentInfo &inFragmentInfo,
                                float initialTempSPH, float coefficientA, float coefficientB) {
  settings.simulationInfoSPH = simulationInfoSph;
  settings.simulationInfoGridFluid = simulationInfoGridFluid;
  settings.gridInfoMC = gridInfoMc;
  settings.initialSPHTemperature = initialTempSPH;
  settings.coefficientA = coefficientA;
  settings.coefficientB = coefficientB;
  fragmentInfo = inFragmentInfo;
}
void SimulationUI::setOnLightSettingsChanged(
    const std::function<void(FragmentInfo)> &onLightSettingsChangedCallback) {
  SimulationUI::onLightSettingsChanged = onLightSettingsChangedCallback;
}
void SimulationUI::setOnMcSettingsChanged(
    const std::function<void(GridInfoMC)> &onMcSettingsChangedCallback) {
  onMCSettingsChanged = onMcSettingsChangedCallback;
}
void SimulationUI::setOnButtonSaveState(const std::function<void()> &onButtonSaveStateCallback) {
  SimulationUI::onButtonSaveState = onButtonSaveStateCallback;
}
void SimulationUI::setOnButtonLoadState(const std::function<void()> &onButtonLoadStateCallback) {
  SimulationUI::onButtonLoadState = onButtonLoadStateCallback;
}
