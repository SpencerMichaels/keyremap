#ifndef KEYREMAP_KEYMAPPER_HPP
#define KEYREMAP_KEYMAPPER_HPP

#include <chrono>
#include <deque>
#include <optional>
#include <unordered_set>
#include <unordered_map>

#include <evdevw.hpp>

#include <DeviceInstance.hpp>

namespace krm {

  class DeviceInstanceProxy;

  using KeyCode = evdevw::event::Key::Code;
  using KeyState = evdevw::event::Key::Value;
  using TimePoint = std::chrono::steady_clock::time_point;
  using Modifiers = std::unordered_set<KeyCode>;

  struct KeyPress {
    KeyCode keycode;
    Modifiers modifiers;
  };

  struct KeyMapping {
    KeyPress from;
    KeyPress to;
    std::optional<KeyPress> to_if_alone;
  };

  class KeyMapper {
  public:
    void add_mapping(const KeyMapping &mapping);

    void operator()(DeviceInstance::Proxy &proxy, evdevw::event::Key event);

  private:
    std::unordered_map <KeyCode, KeyMapping> _mappings;
    std::unordered_set<KeyCode> _keys_combined_held_down;
    std::optional<std::pair<KeyPress, TimePoint>> _key_pending_tap_or_combined;

    bool is_modifier_state_satisfied(DeviceInstance::Proxy &proxy, const Modifiers &modifiers);
    void write_keypress(DeviceInstance::Proxy &proxy, KeyPress keypress, KeyState keystate);
  };

}

#endif //KEYREMAP_KEYMAPPER_HPP
