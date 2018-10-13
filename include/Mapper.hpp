#ifndef KEYREMAP_MAPPER_HPP
#define KEYREMAP_MAPPER_HPP

#include <chrono>
#include <deque>
#include <optional>
#include <unordered_set>
#include <unordered_map>

#include <evdevw.hpp>

namespace krm {

  class WriteEventProxy {
  public:
    void write(evdevw::KeyEvent event);
  };

  using KeyCode = evdevw::KeyEventCode;
  using KeyState = evdevw::KeyEventValue;
  using TimePoint = std::chrono::steady_clock::time_point;

  struct KeyPress {
    KeyCode keycode;
    std::unordered_set<KeyCode> modifiers;

    bool matches(KeyCode kc, const std::unordered_set<KeyCode> &mods) const {
      return keycode == kc && mods == modifiers;
    }
  };

  struct Mapping {
    KeyPress from;
    KeyPress to;
    std::optional <KeyPress> to_if_combined;
  };

  class Mapper {
  public:
    void add_mapping(const Mapping &mapping);

    void map(evdevw::KeyEvent event, WriteEventProxy &proxy);

  private:
    std::unordered_map <KeyCode, Mapping> _mappings;
    std::unordered_set<KeyCode> _keys_held_down;
    std::unordered_map<KeyCode, KeyPress> _keys_combined_held_down;
    std::optional<std::pair<KeyPress, TimePoint>> _key_pending_tap_or_combined;

    void write_keypress(WriteEventProxy &proxy, KeyPress keypress, KeyState keystate);
    void write_key_event(WriteEventProxy &proxy, KeyCode keycode, KeyState keystate);
  };

}

#endif //KEYREMAP_MAPPER_HPP
