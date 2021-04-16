#include <cstdlib>
#include <stdexcept>

#include "spdlog/spdlog.h"

#include "Renderers/TestRenderer.h"
#include "utils/Config.h"


void setupLogger(bool debug = false) {
  if (debug) spdlog::set_level(spdlog::level::debug);
}

int main() {
  Config config("../config.toml");
  setupLogger(config.getApp().DEBUG);

  TestRenderer testRenderer{config};

  try {
    testRenderer.run();
  } catch (const std::exception &e) {
    spdlog::error(fmt::format(e.what()));
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}