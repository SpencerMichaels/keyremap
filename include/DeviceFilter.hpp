#ifndef KEYREMAP_DEVICEFILTER_HPP
#define KEYREMAP_DEVICEFILTER_HPP

#include <regex>

#include <evdevw.hpp>

namespace krm {

  class DeviceFilter {
  public:
    struct Criteria {
      Criteria();

      std::regex name;
      std::regex driver_version;
      std::regex id_bustype;
      std::regex id_product;
      std::regex id_vendor;
      std::regex id_version;
      std::regex phys;
    };

    explicit DeviceFilter(Criteria criteria);

    bool operator()(const evdevw::Evdev &evdev);

  private:
    Criteria _criteria;
  };

}

#endif //KEYREMAP_DEVICEFILTER_HPP
