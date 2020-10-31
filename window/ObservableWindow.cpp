//
// Created by Igor Frank on 31.10.20.
//

#include "ObservableWindow.h"
void ObservableWindow::notifyMouse(const MouseMessage &message) {
    for (auto &[id, listener] : mouseListeners) { listener(message); }
}
void ObservableWindow::notifyKey(const KeyMessage &message) {
    for (auto &[id, listener] : keyListeners) { listener(message); }
}
/*Unsubscriber ObservableWindow::subscribeToKeyEvents(std::invocable<KeyMessage> auto callback) {
    auto id = keyIDs.getNextID();
    keyListeners.emplace(id, callback);
    return Unsubscriber([this, id]() {
        keyListeners.erase(id);
        keyIDs.returnID(id);
    });
}

Unsubscriber ObservableWindow::subscribeToMouseEvents(std::invocable<MouseMessage> auto callback) {
    auto id = mouseIDs.getNextID();
    mouseListeners.emplace(id, callback);
    return Unsubscriber([this, id]() {
        mouseListeners.erase(id);
        mouseIDs.returnID(id);
    });
}*/
