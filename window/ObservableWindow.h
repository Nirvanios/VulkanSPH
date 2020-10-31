//
// Created by Igor Frank on 31.10.20.
//

#ifndef VULKANAPP_OBSERVABLEWINDOW_H
#define VULKANAPP_OBSERVABLEWINDOW_H

#include "Messages.h"
#include <concepts>
#include <functional>
#include <set>
#include <utility>
#include <vector>

class ValuesPool {
public:
    explicit ValuesPool(unsigned int size){
        for (unsigned int i = 0; i < size; ++i) {
            values.emplace(i);
        }
    }
    [[nodiscard]] int getNextID() {
        auto id = *values.begin();
        values.erase(id);
        return id;
    }
    void returnID(int id) { values.insert(id); };

private:
    std::set<int> values;
};

class Unsubscriber {
public:
    explicit Unsubscriber(std::function<void()> unsubFun) : unsub(std::move(unsubFun)){};
    void unsubscribe() { unsub(); };

private:
    std::function<void()> unsub;
};

class ObservableWindow {
public:
    template<std::invocable<KeyMessage> F>
    Unsubscriber subscribeToKeyEvents(F callback){
        auto id = keyIDs.getNextID();
        keyListeners.emplace(id, callback);
        return Unsubscriber([this, id]() {
          keyListeners.erase(id);
          keyIDs.returnID(id);
        });
    }
    template<std::invocable<MouseMessage> F>
    Unsubscriber subscribeToMouseEvents(F callback){
        auto id = mouseIDs.getNextID();
        mouseListeners.emplace(id, callback);
        return Unsubscriber([this, id]() {
          mouseListeners.erase(id);
          mouseIDs.returnID(id);
        });
    }

protected:
    void notifyMouse(const MouseMessage &message);
    void notifyKey(const KeyMessage &message);

private:
    ValuesPool keyIDs{100};
    ValuesPool mouseIDs{100};
    std::unordered_map<int, std::function<void(KeyMessage)>> keyListeners;
    std::unordered_map<int, std::function<void(MouseMessage)>> mouseListeners;
};


#endif//VULKANAPP_OBSERVABLEWINDOW_H
