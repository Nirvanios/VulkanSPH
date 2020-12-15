//
// Created by Igor Frank on 14.12.20.
//

#ifndef VULKANAPP_CAPTURE_H
#define VULKANAPP_CAPTURE_H

#include <future>
class Capture {
  void takeScreenshot(){
    if(waited)
      future = std::async(std::launch::async, [](){/*TODO Save screenshot*/});
    else throw std::runtime_error("It was not waited before saving another frame!");
  }

  void waitForIO(){
      waited = true;
      future.wait();
  };

 private:
  bool waited = true;
  std::future<void> future;

};

#endif//VULKANAPP_CAPTURE_H
