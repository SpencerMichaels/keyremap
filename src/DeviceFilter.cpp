#include <DeviceFilter.hpp>

using krm::DeviceFilter;

DeviceFilter::Criteria::Criteria()
  : name           { ".*", std::regex::optimize | std::regex::nosubs },
    driver_version { ".*", std::regex::optimize | std::regex::nosubs },
    id_bustype     { ".*", std::regex::optimize | std::regex::nosubs },
    id_product     { ".*", std::regex::optimize | std::regex::nosubs },
    id_vendor      { ".*", std::regex::optimize | std::regex::nosubs },
    id_version     { ".*", std::regex::optimize | std::regex::nosubs },
    phys           { ".*", std::regex::optimize | std::regex::nosubs }
{
}

DeviceFilter::DeviceFilter(DeviceFilter::Criteria criteria)
  : _criteria(std::move(criteria))
{
}

bool krm::DeviceFilter::operator()(const evdevw::Evdev &evdev) {
  const auto phys = evdev.get_phys();

  return (std::regex_match(evdev.get_name(), _criteria.name) &&
          std::regex_match(std::to_string(evdev.get_driver_version()), _criteria.driver_version) &&
          std::regex_match(std::to_string(evdev.get_id_bustype()), _criteria.id_bustype) &&
          std::regex_match(std::to_string(evdev.get_id_product()), _criteria.id_product) &&
          std::regex_match(std::to_string(evdev.get_id_vendor()), _criteria.id_vendor) &&
          std::regex_match(std::to_string(evdev.get_id_version()), _criteria.id_version) &&
          std::regex_match(phys ? *phys : "", _criteria.phys));
}
