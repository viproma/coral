#include "coral/bus/slave_setup.hpp"

#include <cassert>
#include <limits>


namespace coral
{
namespace bus
{


SlaveSetup::SlaveSetup()
    : startTime(std::numeric_limits<coral::model::TimePoint>::signaling_NaN()),
      stopTime(std::numeric_limits<coral::model::TimePoint>::signaling_NaN())
{
}


SlaveSetup::SlaveSetup(
    coral::model::TimePoint startTime_,
    coral::model::TimePoint stopTime_,
    const std::string& executionName_,
    std::chrono::milliseconds variableRecvTimeout_)
    : startTime(startTime_),
      stopTime(stopTime_),
      executionName(executionName_),
      variableRecvTimeout(variableRecvTimeout_)
{
    assert(startTime <= stopTime);
    assert(variableRecvTimeout > std::chrono::milliseconds(0));
}


}} // namespace
