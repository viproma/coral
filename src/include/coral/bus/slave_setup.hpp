/**
\file
\brief  Defines the coral::bus::SlaveSetup class
*/
#ifndef CORAL_BUS_SLAVE_SETUP_HPP
#define CORAL_BUS_SLAVE_SETUP_HPP

#include "coral/model.hpp"


namespace coral
{
namespace bus
{


/**
\brief  Configuration data which is sent to each slave as they are added
        to the simulation.
*/
struct SlaveSetup
{
    SlaveSetup();
    SlaveSetup(
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        const std::string& executionName);
    coral::model::TimePoint startTime;
    coral::model::TimePoint stopTime;
    std::string executionName;
};


}} // namespace
#endif // header guard
