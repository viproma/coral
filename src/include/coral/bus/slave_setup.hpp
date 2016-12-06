/**
\file
\brief  Defines the coral::bus::SlaveSetup class
*/
#ifndef CORAL_BUS_SLAVE_SETUP_HPP
#define CORAL_BUS_SLAVE_SETUP_HPP

#include <chrono>
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
        const std::string& executionName,
        std::chrono::milliseconds variableRecvTimeout);
    coral::model::TimePoint startTime;
    coral::model::TimePoint stopTime;
    std::string executionName;

    /**
    \brief  How long a slave should wait to receive variable values from
            other slaves before assuming that the connection is broken or
            that a subscription has failed to take effect.
    */
    std::chrono::milliseconds variableRecvTimeout;
};


}} // namespace
#endif // header guard
