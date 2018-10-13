#ifndef KEYREMAP_DEVICEINSTANCE_HPP
#define KEYREMAP_DEVICEINSTANCE_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>

#include <evdevw.hpp>

namespace krm {

  class DeviceInstance {
  public:
    explicit DeviceInstance(std::shared_ptr<evdevw::Evdev> evdev);
    ~DeviceInstance();

    void grab();
    void ungrab();

    template <typename F>
    void process_pending_events(const F &f) {
    }

  private:
    std::shared_ptr<evdevw::Evdev> _evdev;
    evdevw::UInput _uinput;
    bool _grabbed;
  };

}

#endif //KEYREMAP_DEVICEINSTANCE_HPP
