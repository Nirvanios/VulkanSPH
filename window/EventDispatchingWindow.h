//
// Created by Igor Frank on 31.10.20.
//

#ifndef VULKANAPP_EVENTDISPATCHINGWINDOW_H
#define VULKANAPP_EVENTDISPATCHINGWINDOW_H

#include "../utils/Utilities.h"
#include "Messages.h"
#include <concepts>
#include <functional>
#include <set>
#include <utility>
#include <vector>

class Unsubscriber {
 public:
  explicit Unsubscriber(std::function<void()> unsubFun) : unsub(std::move(unsubFun)){};
  void unsubscribe() { unsub(); };

 private:
  std::function<void()> unsub;
};

class EventDispatchingWindow {
 public:
  template<std::invocable<KeyMessage> F>
  [[nodiscard]] Unsubscriber subscribeToKeyEvents(F callback) {
    auto id = IDs.getNextID();
    keyListeners.emplace(id, callback);
    return Unsubscriber([this, id]() { keyListeners.erase(id); });
  }
  template<std::invocable<MouseButtonMessage> F>
  [[nodiscard]] Unsubscriber subscribeToMouseButtonEvents(F callback) {
    auto id = IDs.getNextID();
    mouseButtonListeners.emplace(id, callback);
    return Unsubscriber([this, id]() { mouseButtonListeners.erase(id); });
  }
  template<std::invocable<MouseMovementMessage> F>
  [[nodiscard]] Unsubscriber subscribeToMouseMovementEvents(F callback) {
    auto id = IDs.getNextID();
    mouseMovementListeners.emplace(id, callback);
    return Unsubscriber([this, id]() { mouseMovementListeners.erase(id); });
  }

  void setIgnorePredicate(std::function<bool()> predicate);

 protected:
  void notifyMouseButton(const MouseButtonMessage &message);
  void notifyMouseMovement(const MouseMovementMessage &message);
  void notifyKey(const KeyMessage &message);

 private:
  Utilities::IdGenerator IDs;
  std::unordered_map<int, std::function<void(KeyMessage)>> keyListeners;
  std::unordered_map<int, std::function<void(MouseButtonMessage)>> mouseButtonListeners;
  std::unordered_map<int, std::function<void(MouseMovementMessage)>> mouseMovementListeners;

  std::function<bool()> ignorePredicate = [](){return false;};
};

#endif//VULKANAPP_EVENTDISPATCHINGWINDOW_H
