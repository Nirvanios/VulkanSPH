//
// Created by Igor Frank on 31.10.20.
//

#include "EventDispatchingWindow.h"
void EventDispatchingWindow::notifyMouseButton(const MouseButtonMessage &message) {
    for (auto &[id, listener] : mouseButtonListeners) { listener(message); }
}
void EventDispatchingWindow::notifyKey(const KeyMessage &message) {
    for (auto &[id, listener] : keyListeners) { listener(message); }
}
void EventDispatchingWindow::notifyMouseMovement(const MouseMovementMessage &message) {
    for (auto &[id, listener] : mouseMovementListeners) { listener(message); }
}
