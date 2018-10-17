#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <vector>

#include <udevw.hpp>

#include <DeviceFilter.hpp>
#include <DeviceInstance.hpp>
#include <KeyMapper.hpp>

std::vector<std::unique_ptr<krm::DeviceInstance>> _instances;

int main(int argc, char **argv) {
  auto udev = udevw::Udev::create();
  auto enumerate = udevw::Enumerate::create(udev);
  enumerate.add_match_subsystem("input");

  const auto devices = enumerate.scan_devices();

  krm::DeviceFilter::Criteria criteria;
  criteria.name.assign(".*Gaming Mouse.*");

  krm::DeviceFilter filter(criteria);

  krm::KeyMapping mapping {
    .from = krm::KeyPress {
      .keycode = krm::KeyCode::Capslock,
      .modifiers = {} },
    .to = krm::KeyPress {
      .keycode = krm::KeyCode::Leftctrl,
      .modifiers = {} },
    .to_if_alone = krm::KeyPress {
      .keycode = krm::KeyCode::Esc,
      .modifiers = {} },
  };

  krm::KeyMapper mapper;
  mapper.add_mapping(mapping);

  std::cout << "===== Scanning devices =====" << std::endl;
  for (const auto &entry : devices) {
    auto device = udevw::Device::create_from_syspath(udev, entry.name);
    auto devnode = device.get_devnode();

    if (!devnode)
      continue;

    try {
      auto fd = open(devnode->c_str(), O_RDONLY);
      auto evdev = evdevw::Evdev::create_from_fd(fd);

      if (filter(*evdev)) {
        std::cout << "MATCH: " << evdev->get_name() << " " << *devnode << std::endl;

        auto devinst = std::make_unique<krm::DeviceInstance>(evdev);
        devinst->grab();
        _instances.push_back(std::move(devinst));
      } else {
        std::cout << "no match: " << evdev->get_name() << " " << *devnode << std::endl;
      }
    } catch (const evdevw::Exception &e) {
      std::cout << "Invalid device: " << *devnode << std::endl;
      std::cout << std::strerror(e.get_error()) << std::endl;
      continue;
    }
  }

  std::cout << "===== Running =====" << std::endl;
  try {
    while (true) {
      for (auto &devinst : _instances) {
        devinst->process_event([&](auto &proxy, const auto &event) {
          std::visit([](const auto &event) {
            std::cout << event.get_type_name() << " "
                      << event.get_code_name() << " "
                      << event.get_raw_value() << std::endl;
          }, event);

          if (std::holds_alternative<evdevw::event::Key>(event)) {
            mapper(proxy, std::get<evdevw::event::Key>(event));
          } else {
            proxy.write_event(std::move(event));
          }
        });
      }
    }
  } catch (const evdevw::Exception &e) {
    std::cout << std::strerror(e.get_error()) << std::endl;
  }
}
