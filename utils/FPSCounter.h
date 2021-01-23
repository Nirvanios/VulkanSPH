//
// Created by Igor Frank on 22.01.21.
//

#ifndef VULKANAPP_FPSCOUNTER_H
#define VULKANAPP_FPSCOUNTER_H

#include <chrono>
#include <functional>
#include <plf_nanotimer.h>

class FPSCounter {

 public:
  FPSCounter();
  void newFrame();

  void setOnNewFrameCallback(std::function<void()> callback);

  double getCurrentFPS();
  double getAverageFPS();

  double getCurrentFrameTime();
  double getAverageFrameTime();


 private:
  plf::nanotimer nanotimer;

  unsigned int frameCount{};
  double allFrames = 0;
  double lastFrameDuration;

  std::function<void()> onNewFrame;
};

#endif//VULKANAPP_FPSCOUNTER_H
