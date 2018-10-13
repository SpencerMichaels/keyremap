#include <Mapper.hpp>

using evdevw::KeyEvent;
using krm::KeyCode;
using krm::KeyState;
using krm::Mapper;

static std::chrono::steady_clock::duration TIMEOUT_DURATION(200); // TODO

void Mapper::add_mapping(const Mapping &mapping) {
  _mappings[mapping.from.keycode] = mapping;
}

void Mapper::map(evdevw::KeyEvent event, WriteEventProxy &proxy) {
  const auto keycode = event.get_code();
  const auto keystate = event.get_value();

  // If the previous key event was pressing a key with a .to_if_combined mapping down,
  // we need to use the current event to decide if the last event should turn into
  // tapping .to or holding .to_if_combined.
  if (_key_pending_tap_or_combined) {
    if (_key_pending_tap_or_combined->first.matches(keycode, _keys_held_down)) {
      if (keystate != KeyState::Up)
        return;

      const auto now = std::chrono::steady_clock::now();
      const auto time_since_down = now - _key_pending_tap_or_combined->second;

      _key_pending_tap_or_combined = std::nullopt;

      // tapping .from becomes .to only if the down-up presses happen within a time window
      if (time_since_down <= TIMEOUT_DURATION) {
        write_keypress(proxy, _mappings.at(keycode).to, KeyState::Down);
        write_keypress(proxy, _mappings.at(keycode).to, KeyState::Up);
      }
      return;
    } else {
      // start holding down .to_if_combined, record it so we can let go later
      const auto keypress = _mappings.at(keycode).to_if_combined.value();
      _keys_combined_held_down.emplace(keycode, keypress);
      write_keypress(proxy, keypress, KeyState::Down);

      // forward the original event now that the keys are held down
      write_key_event(proxy, keycode, keystate);
      return;
    }
  }

  // If this event represents a previously-down .from key with a .to_if_combined mapping
  // coming up, or one of .from's modifiers coming up, then send a key-up event for
  // the .to_if_combined mapping and clear it from the combined held-down list.
  if (keystate == KeyState::Up) {
    auto _ = std::remove_if(_keys_combined_held_down.begin(), _keys_combined_held_down.end(),
      [&](const auto &kv) {
        const auto &[cmb_keycode, cmb_keypress] = kv;
        const auto &modifiers = _mappings.at(keycode).from.modifiers;
        if ((cmb_keycode == keycode) || modifiers.count(cmb_keycode)) {
          write_keypress(proxy, cmb_keypress, KeyState::Up);
          return true;
        }
        return false;
      });
  }

  // Otherwise, it's either a simple key-to-key mapping or the down-press of a
  // .to_if_combined mapping.
  if (_mappings.count(keycode) &&
      _mappings.at(keycode).from.matches(keycode, _keys_held_down))
  {
    auto &mapping = _mappings.at(keycode);
    if (mapping.to_if_combined && keystate == KeyState::Down) {
      _key_pending_tap_or_combined.emplace(mapping.from, std::chrono::steady_clock::now());
    } else {
      write_keypress(proxy, mapping.to, keystate);
    }
  }
}

void Mapper::write_key_event(WriteEventProxy &proxy, KeyCode keycode, KeyState keystate) {
  proxy.write(KeyEvent(keycode, keystate));

  switch (keystate) {
    case KeyState::Down: {
      _keys_held_down.insert(keycode);
    }; break;
    case KeyState::Up: {
      _keys_held_down.erase(keycode);
    }; break;
    default:
      break;
  }
}

void Mapper::write_keypress(WriteEventProxy &proxy, KeyPress keypress, KeyState keystate) {
  const auto write_modifiers = [&]() {
    for (const auto keycode : keypress.modifiers) {
      write_key_event(proxy, keycode, keystate);
    }
  };

  if (keystate == KeyState::Down) {
    write_modifiers();
  }

  write_key_event(proxy, keypress.keycode, keystate);

  if (keystate == KeyState::Up) {
    write_modifiers();
  }
}
