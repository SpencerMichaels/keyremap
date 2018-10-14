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
    class Proxy {
    public:
      explicit Proxy(DeviceInstance &device_instance)
        : _device_instance(device_instance)
      {
      }

      void write_event(evdevw::event::AnyEvent event) {
        _device_instance.write_event(std::move(event));
      }

      template <typename Code>
      typename evdevw::event::event_from_event_code<Code>::type::Value
      get_event_value(Code code) {
        return _device_instance._uinput_evdev->get_event_value(code);
      }

    private:
      DeviceInstance &_device_instance;
    };

  public:
    explicit DeviceInstance(std::shared_ptr<evdevw::Evdev> evdev);
    ~DeviceInstance();

    void grab();
    void ungrab();

    void write_event(evdevw::event::AnyEvent event);

    template <typename F>
    void process_event(const F &f) {
      using ReadFlags = std::unordered_set<evdevw::ReadFlag>;

      static const ReadFlags READ_FLAGS_NORMAL = {
          evdevw::ReadFlag::Normal, evdevw::ReadFlag::Blocking };
      static const ReadFlags READ_FLAGS_SYNC = {
          evdevw::ReadFlag::Sync };

      if (auto ret = _evdev->next_event(_needs_sync ? READ_FLAGS_SYNC : READ_FLAGS_NORMAL)) {
        auto &[status, event] = *ret;

        _needs_sync = (status == evdevw::ReadStatus::Sync);

        Proxy proxy(*this);
        f(proxy, event);
      }
    }

  private:
    std::shared_ptr<evdevw::Evdev> _evdev, _uinput_evdev;
    evdevw::UInput _uinput;
    bool _grabbed;

    std::unordered_set<uint16_t> _keycodes_since_last_syn;
    bool _needs_sync;
  };

}

#endif //KEYREMAP_DEVICEINSTANCE_HPP
