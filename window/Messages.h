//
// Created by Igor Frank on 31.10.20.
//

#ifndef VULKANAPP_MESSAGES_H
#define VULKANAPP_MESSAGES_H

#include <GLFW/glfw3.h>
#include <type_traits>
enum class Modifier {
  None = 0x0,
  Shift = GLFW_MOD_SHIFT,
  Control = GLFW_MOD_CONTROL,
  Super = GLFW_MOD_SUPER,
  CapsLock = GLFW_MOD_CAPS_LOCK,
  NumLock = GLFW_MOD_NUM_LOCK
};

enum class KeyAction {
  Uknown = GLFW_KEY_UNKNOWN,
  Press = GLFW_PRESS,
  Repeat = GLFW_REPEAT,
  Release = GLFW_RELEASE
};

enum class MouseButtonAction { Press = GLFW_PRESS, Release = GLFW_RELEASE };

inline Modifier operator|(Modifier lhs, Modifier rhs) {
  using T = std::underlying_type_t<Modifier>;
  return static_cast<Modifier>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline Modifier &operator|=(Modifier &lhs, Modifier rhs) {
  lhs = lhs | rhs;
  return lhs;
}

struct KeyMessage {
  int key;
  int scancode;
  KeyAction action;
  Modifier modifier;
};

struct MouseButtonMessage {
  int button;
  MouseButtonAction action;
  Modifier modifier;
};

struct MouseMovementMessage {
  double xPosition;
  double yPosition;
};

#endif//VULKANAPP_MESSAGES_H
