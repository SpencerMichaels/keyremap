#include <DeviceInstance.hpp>

using krm::DeviceInstance;

DeviceInstance::DeviceInstance(std::shared_ptr<evdevw::Evdev> evdev)
  : _evdev(std::move(evdev)),
    _uinput(*_evdev),
    _grabbed(false)
{
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

