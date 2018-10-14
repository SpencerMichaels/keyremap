#include <chrono>
#include <functional>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <queue>
#include <unordered_map>

#include <evdevw.hpp>
#include <udevw.hpp>

using KeyCode = evdevw::KeyEventCode;
using Modifiers = std::set<evdevw::KeyEventCode>;

struct KeyPress {
  KeyCode keycode;
  Modifiers modifiers;

  bool matches(KeyCode kc, Modifiers mods) {
    return keycode == kc && mods == modifiers;
  }
};

struct Mapping {
  KeyPress from;
  KeyPress to;
  std::optional<KeyPress> to_if_combined;
};

class DeviceMonitor {
public:
  using OnDeviceEventFn = std::function<void(udevw::Device&)>;

  DeviceMonitor()
    : _udev(udevw::Udev::create()),
      _enumerate(udevw::Enumerate::create(_udev)),
      _monitor(udevw::Monitor::create_from_netlink(_udev, "udev"))
  {
    _enumerate.add_match_subsystem("input");
    _monitor.filter_add_match_subsystem("input");
  }

  void on_add(OnDeviceEventFn f) { _on_add = std::move(f); }
  void on_change(OnDeviceEventFn f) { _on_change = std::move(f); }
  void on_offline(OnDeviceEventFn f) { _on_offline = std::move(f); }
  void on_online(OnDeviceEventFn f) { _on_online = std::move(f); }
  void on_remove(OnDeviceEventFn f) { _on_remove = std::move(f); }

  void run() {
    const auto devices = _enumerate.scan_devices();

    for (const auto &entry : devices) {
      auto device = udevw::Device::create_from_syspath(_udev, entry.name);
      if (_on_add)
        _on_add(device);
    }

    _monitor.enable_receiving();
    const auto fd = _monitor.get_fd();

    for (;;) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(fd, &fds);

      if (select(fd + 1, &fds, nullptr, nullptr, nullptr) > 0 && FD_ISSET(fd, &fds)) {
        auto device = _monitor.receive_device();

        const auto action = device.get_action();
        if (!action)
          continue;

        if (*action == "add" && _on_add)
          _on_add(device);
        else if (*action == "change" && _on_change)
          _on_change(device);
        else if (*action == "offline" && _on_offline)
          _on_offline(device);
        else if (*action == "online" && _on_online)
          _on_online(device);
        else if (*action == "remove" && _on_remove)
          _on_remove(device);
      }
    }
  }

private:
  udevw::Udev::Pointer _udev;
  udevw::Enumerate _enumerate;
  udevw::Monitor _monitor;

  OnDeviceEventFn _on_add, _on_remove, _on_change, _on_online, _on_offline;
};

class DeviceContext {
public:
  DeviceContext(std::shared_ptr<evdevw::Evdev> evdev)
    : _evdev(evdev),
      _uinput(*evdev),
  {
  };

  ~DeviceContext() {
    if (_grabbed)
      ungrab();
  }

  void grab() {
    _attached = true;
    _evdev->grab();
  }

  void ungrab() {
    _evdev->ungrab();
    _grabbed = false;
  }

  bool process_event() {
    auto ret = _evdev->next_event({ evdevw::ReadFlag::Normal, evdevw::ReadFlag::Blocking });

    if (!ret)
      return false;

    auto [status, event] = *ret;

    while (status == evdevw::ReadStatus::Sync) {
      ret = _evdev->next_event({ evdevw::ReadFlag::Sync });

      if (!ret)
        return false; // TODO: OK?

      status = ret->first;
      event = ret->second;
    }

    // Don't handle non key-events, just forward them
    if (!std::holds_alternative<evdevw::KeyEvent>) {
      _uinput.write_event(event);
      return true;
    }

    return true;
  }

private:
  std::shared_ptr<evdevw::Evdev> _evdev;
  evdevw::UInput _uinput;
  bool _grabbed;

  std::unordered_map<evdevw::KeyEventCode, Mapping> _mappings;
  std::set<evdevw::KeyEventCode> _keys_held_down;
  std::optional<std::pair<evdevw::KeyEventCode, std::chrono::time>>
};

std::unordered_map<std::string, DeviceContext> _contexts;

int main() {
  try {
    DeviceMonitor monitor;

    monitor.on_add([](const auto &device) {
      auto syspath = device.get_syspath();
      if (!syspath)
        return;

      std::cout << *syspath << std::endl;

      auto devnode = device.get_devnode();
      if (!devnode)
        return;
      std::cout << "  " << *devnode << std::endl;

      if (syspath->find("event8") != std::string::npos) {
        auto fd = open(devnode->c_str(), O_RDONLY);
        auto evdev = evdevw::Evdev::create_from_fd(fd);
        auto dc = DeviceContext(evdev);
        dc.attach();
        while (true) {
          dc.process_event();
        }
      }
    });

    monitor.run();
  } catch (const evdevw::Exception &e) {
    std::cout << "Error: " << std::strerror(e.get_error()) << std::endl;
  }
}
