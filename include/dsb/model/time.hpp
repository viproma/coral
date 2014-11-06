/**
\file
\brief Types and constants related to simulation time.
*/
#ifndef DSB_MODEL_TYPES_HPP
#define DSB_MODEL_TYPES_HPP

#include <limits>


namespace dsb
{
namespace model
{


/// The type used to specify (simulation) time points.
typedef double TimePoint;


/// A special TimePoint value that lies infinitely far in the future.
const TimePoint ETERNITY = std::numeric_limits<TimePoint>::infinity();


/**
\brief  The type used to specify (simulation) time durations.

If `t1` and `t2` have type TimePoint, then `t2-t1` has type TimeDuration.
If `t` has type TimePoint and `dt` has type TimeDuration, then `t+dt` has type
TimePoint.
*/
typedef double TimeDuration;


}}      // namespace
#endif  // header guard
