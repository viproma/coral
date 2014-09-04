#ifndef DSB_SLAVE_MOCK_SLAVES_HPP
#define DSB_SLAVE_MOCK_SLAVES_HPP

#include <memory>
#include <string>
#include "dsb/bus/slave_agent.hpp"

std::unique_ptr<dsb::bus::ISlaveInstance> NewSlave(const std::string& type);


#endif  // header guard
