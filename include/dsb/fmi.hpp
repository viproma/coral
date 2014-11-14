/**
\file
\brief Main header file for dsb::fmi.
*/
#ifndef DSB_FMI_HPP
#define DSB_FMI_HPP

#include <memory>
#include <ostream> //TODO: Temporary; remove when we have proper observers.
#include "boost/filesystem/path.hpp"
#include "dsb/domain/slave_provider.hpp"
#include "dsb/execution/slave.hpp"


namespace dsb
{
/// FMI-based slaves and slave providers.
namespace fmi
{


/**
\brief Makes a new slave type based on an FMU.
*/
std::unique_ptr<dsb::domain::ISlaveType> MakeSlaveType(
    const boost::filesystem::path& fmu);


/**
\brief  Makes a new slave instance based on an FMU.
*/
std::unique_ptr<dsb::execution::ISlaveInstance> MakeSlaveInstance(
    const boost::filesystem::path& fmu,
    std::ostream* outputStream = nullptr); // TODO: Temporary; remove when we have proper observers.


}}
#endif  // header guard
