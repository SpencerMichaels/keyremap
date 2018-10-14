#include <iostream>

#include <KeyMapper.hpp>

using krm::KeyCode;
using krm::KeyState;
using krm::KeyMapper;

using KeyEvent = evdevw::event::Key;

static std::chrono::milliseconds TIMEOUT_DURATION(200); // TODO

void KeyMapper::add_mapping(const KeyMapping &mapping) {
  _mappings[mapping.from.keycode] = mapping;
}

void KeyMapper::operator()(DeviceInstance::Proxy &proxy, KeyEvent event) {
  const auto keycode = event.get_code();
  const auto keystate = event.get_value();

  // If the previous key event was pressing a key with a .to_if_alone mapping down,
  // we need to use the current event to decide if the last event should turn into
  // holding .to or tapping .to_if_alone.
  if (_key_pending_tap_or_combined) {
    const auto target_keypress = _key_pending_tap_or_combined->first;

    if (target_keypress.keycode == keycode &&
        is_modifier_state_satisfied(proxy, target_keypress.modifiers))
    {
      if (keystate != KeyState::Up)
        return;

      const auto now = std::chrono::steady_clock::now();
      const auto time_since_down = now - _key_pending_tap_or_combined->second;
      std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(time_since_down).count() << std::endl;

      // tapping .from becomes .to_if_alone only if the down-up presses happen within a time window
      if (time_since_down <= TIMEOUT_DURATION) {
        std::cout << "not timed out" << std::endl;
        const auto &keypress = *_mappings.at(keycode).to_if_alone;
        write_keypress(proxy, keypress, KeyState::Down);
        write_keypress(proxy, keypress, KeyState::Up);
      } else {
        std::cout << "TIMED OUT" << std::endl;
      }
    } else {
      // start holding down .to, record it so we can let go later
      const auto from_keycode = _key_pending_tap_or_combined->first.keycode;
      const auto &keypress = _mappings.at(from_keycode).to;
      _keys_combined_held_down.insert(from_keycode);
      write_keypress(proxy, keypress, KeyState::Down);

      // forward the original event now that the keys are held down
      proxy.write_event(KeyEvent(keycode, keystate));
    }

    _key_pending_tap_or_combined = std::nullopt;
    return;
  }

  // If this event represents a previously-down .from key with a .to_if_alone mapping
  // coming up, or one of .from's modifiers coming up, then send a key-up event for
  // the .to mapping and clear it from the combined held-down list.
  if (keystate == KeyState::Up) {
    auto it = _keys_combined_held_down.begin();
    while (it != _keys_combined_held_down.end()) {
      const auto &from_keycode = *it;
      const auto &modifiers = _mappings.at(from_keycode).from.modifiers;
      if ((keycode == from_keycode) || modifiers.count(keycode)) {
        write_keypress(proxy, _mappings.at(from_keycode).to, KeyState::Up);
        it = _keys_combined_held_down.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Check if the event matches a simple key-to-key mapping or the down-press of a
  // .to_if_combined mapping.
  if (_mappings.count(keycode) &&
      _mappings.at(keycode).from.keycode == keycode &&
      is_modifier_state_satisfied(proxy, _mappings.at(keycode).from.modifiers))
  {
    auto &mapping = _mappings.at(keycode);

    if (mapping.to_if_alone && keystate == KeyState::Down) {
      _key_pending_tap_or_combined.emplace(mapping.from, std::chrono::steady_clock::now());
    } else if (mapping.to_if_alone && keystate == KeyState::Repeat) {
      write_keypress(proxy, mapping.to, keystate);
    } else {
      write_keypress(proxy, mapping.to, keystate);
    }

    return;
  }

  // Otherwise, just forward the event
  proxy.write_event(KeyEvent(keycode, keystate));
}

bool KeyMapper::is_modifier_state_satisfied(DeviceInstance::Proxy &proxy, const Modifiers &modifiers) {
  return std::all_of(modifiers.begin(), modifiers.end(), [&](const auto &modifier) {
    return proxy.get_event_value(modifier) != KeyState::Up;
  });
}

void KeyMapper::write_keypress(DeviceInstance::Proxy &proxy, KeyPress keypress, KeyState keystate) {
  const auto write_modifiers = [&]() {
    for (const auto keycode : keypress.modifiers)
      proxy.write_event(KeyEvent(keycode, keystate));
  };

  if (keystate == KeyState::Down)
    write_modifiers();

  proxy.write_event(KeyEvent(keypress.keycode, keystate));

  if (keystate == KeyState::Up)
    write_modifiers();
}
