#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <DeviceInstance.hpp>
#include <unistd.h>

using krm::DeviceInstance;

DeviceInstance::DeviceInstance(std::shared_ptr<evdevw::Evdev> evdev)
  : _evdev(std::move(evdev)),
    _uinput(*_evdev),
    _grabbed(false),
    _needs_sync(false)
{
  const auto devnode = _uinput.get_devnode();
  if (!devnode)
    throw std::runtime_error("Couldn't get devnode for uinput device!");

  auto fd = open(devnode->c_str(), O_RDONLY);
  if (fd < 0)
    throw std::runtime_error("Couldn't open devnode FD for uinput device!");

  _uinput_evdev = evdevw::Evdev::create_from_fd(fd);
};

DeviceInstance::~DeviceInstance() {
  if (_grabbed)
    ungrab();
}

void DeviceInstance::grab() {
  _grabbed = true;
  _evdev->grab();
}

void DeviceInstance::ungrab() {
  _evdev->ungrab();
  _grabbed = false;
}

void DeviceInstance::write_event(evdevw::event::AnyEvent event) {
  using KeyEvent = evdevw::event::Key;
  using SynEvent = evdevw::event::Synchronize;

  if (std::holds_alternative<SynEvent>(event)) {
    _keycodes_since_last_syn.clear();
  } else if (std::holds_alternative<KeyEvent>(event)) {
    const auto &key_event = std::get<KeyEvent>(event);
    const auto keycode = key_event.get_raw_code();

    if (_keycodes_since_last_syn.find(keycode) != _keycodes_since_last_syn.end()) {
      _keycodes_since_last_syn.clear();
      _uinput.write_event(SynEvent(SynEvent::Code::Report, 0));
      usleep(20000);
    }

    _keycodes_since_last_syn.insert(keycode);
  }

  _uinput.write_event(std::move(event));
}
