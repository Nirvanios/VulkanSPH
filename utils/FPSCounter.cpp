//
// Created by Igor Frank on 22.01.21.
//

#include "FPSCounter.h"

#include <utility>

using namespace std::chrono;

void FPSCounter::newFrame() {
  lastFrameDuration = nanotimer.get_elapsed_ms();
  ++frameCount;
  allFrames += lastFrameDuration;
  onNewFrame();
  nanotimer.start();
}

void FPSCounter::setOnNewFrameCallback(std::function<void()> callback) {
  onNewFrame = std::move(callback);
}

double FPSCounter::getCurrentFPS() { return 1 / (lastFrameDuration / 1000.0); }

double FPSCounter::getAverageFPS() {
  return (static_cast<double>(frameCount) / (allFrames / 1000.0));
}

double FPSCounter::getCurrentFrameTime() { return lastFrameDuration; }

double FPSCounter::getAverageFrameTime() { return allFrames / frameCount; }

FPSCounter::FPSCounter(){nanotimer.start();}
