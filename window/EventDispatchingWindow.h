//
// Created by Igor Frank on 31.10.20.
//

#ifndef VULKANAPP_EVENTDISPATCHINGWINDOW_H
#define VULKANAPP_EVENTDISPATCHINGWINDOW_H

#include "../Utilities.h"
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
    Unsubscriber subscribeToKeyEvents(F callback){
        auto id = keyIDs.getNextID();
        keyListeners.emplace(id, callback);
        return Unsubscriber([this, id]() {
          keyListeners.erase(id);
        });
    }
    template<std::invocable<MouseMessage> F>
    Unsubscriber subscribeToMouseEvents(F callback){
        auto id = mouseIDs.getNextID();
        mouseListeners.emplace(id, callback);
        return Unsubscriber([this, id]() {
          mouseListeners.erase(id);
        });
    }

protected:
    void notifyMouse(const MouseMessage &message);
    void notifyKey(const KeyMessage &message);

private:
    Utilities::IdGenerator keyIDs{100};
    Utilities::IdGenerator mouseIDs{100};
    std::unordered_map<int, std::function<void(KeyMessage)>> keyListeners;
    std::unordered_map<int, std::function<void(MouseMessage)>> mouseListeners;
};


#endif//VULKANAPP_EVENTDISPATCHINGWINDOW_H
