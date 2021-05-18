#include <argparse.hpp>
#include <cstdlib>
#include <stdexcept>

#include "spdlog/spdlog.h"

#include "Renderers/SimulatorRenderer.h"
#include "utils/Config.h"

void setupLogger(bool debug = false) {
  if (debug) spdlog::set_level(spdlog::level::debug);
}

int main(int argc, char **argv) {
  argparse::ArgumentParser program("Fluid simulation");
  program.add_argument("-c", "--config")
      .help("Path to file with simulation configuration.")
      .default_value(std::filesystem::path("./config.toml"))
      .action([](const auto &value) { return std::filesystem::path(value); });

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cout << err.what() << std::endl;
    std::cout << program;
    return EXIT_FAILURE;
  }

  Config config(program.get<std::filesystem::path>("-c"));
  setupLogger(config.getApp().DEBUG);

  SimulatorRenderer testRenderer{config};

  try {
    testRenderer.run();
  } catch (const std::exception &e) {
    spdlog::error(fmt::format(e.what()));
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}