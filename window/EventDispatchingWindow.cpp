//
// Created by Igor Frank on 31.10.20.
//

#include "EventDispatchingWindow.h"

#include <utility>
void EventDispatchingWindow::notifyMouseButton(const MouseButtonMessage &message) {
  if(not ignorePredicate()) for (auto &[id, listener] : mouseButtonListeners) { listener(message); }
}
void EventDispatchingWindow::notifyKey(const KeyMessage &message) {
  if(not ignorePredicate()) for (auto &[id, listener] : keyListeners) { listener(message); }
}
void EventDispatchingWindow::notifyMouseMovement(const MouseMovementMessage &message) {
  if(not ignorePredicate()) for (auto &[id, listener] : mouseMovementListeners) { listener(message); }
}

void EventDispatchingWindow::setIgnorePredicate(std::function<bool()> predicate) {
  ignorePredicate = std::move(predicate);
}
