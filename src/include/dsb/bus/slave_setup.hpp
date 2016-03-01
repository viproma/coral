/**
\file
\brief  Defines the dsb::bus::SlaveSetup class
*/
#ifndef DSB_BUS_SLAVE_SETUP_HPP
#define DSB_BUS_SLAVE_SETUP_HPP

#include "dsb/model.hpp"


namespace dsb
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
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime,
        const std::string& variablePubEndpoint,
        const std::string& variableSubEndpoint,
        const std::string& executionName);
    dsb::model::TimePoint startTime;
    dsb::model::TimePoint stopTime;
    std::string variablePubEndpoint;
    std::string variableSubEndpoint;
    std::string executionName;
};


}} // namespace
#endif // header guard
