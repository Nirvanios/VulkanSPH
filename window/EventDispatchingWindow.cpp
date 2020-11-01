//
// Created by Igor Frank on 31.10.20.
//

#include "EventDispatchingWindow.h"
void EventDispatchingWindow::notifyMouse(const MouseMessage &message) {
    for (auto &[id, listener] : mouseListeners) { listener(message); }
}
void EventDispatchingWindow::notifyKey(const KeyMessage &message) {
    for (auto &[id, listener] : keyListeners) { listener(message); }
}
