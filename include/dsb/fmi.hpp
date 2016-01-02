/**
\file
\brief Main header file for dsb::fmi.
*/
#ifndef DSB_FMI_HPP
#define DSB_FMI_HPP

#include <chrono>
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
\brief  The function type used for the "slave starter function" argument to
        MakeSlaveType().

See MakeSlaveType() for details.
*/
typedef std::function<dsb::net::SlaveLocator(const std::string&, std::chrono::milliseconds)>
    SlaveStarter;


/**
\brief Makes a new slave type based on an FMU.

The files in the FMU will be unpacked to a temporary directory, which will
be automatically deleted again when the returned object is destroyed.

The returned object must have the ability to spawn a new slave instance.
This is done via a user-supplied function whose signature is defined by the
dsb::fmi::SlaveStarter typedef.
This function should create a new slave instance using MakeSlaveInstance() and
execute it with dsb::execution::RunSlave(), typically in a new thread or in a
separate process, or wherever is deemed appropriate for the client program.
(This flexibility is the reason for why the function must be supplied by the
caller.)  The function must have the following signature:
~~~{.cpp}
dsb::net::SlaveLocator MySlaveStarter(const std::string& fmu);
~~~
The input argument will be the path to the FMU, and the function should return
an object that allows the master to locate the instantiated slave.
The function may throw an exception derived from std::runtime_error if it fails
to instantiate and/or connect the slave.

\param [in] fmu
    The FMU file path.
\param [in] slaveStarterFunction
    The function that is called by the dsb::domain::ISlaveType::InstantiateAndConnect()
    method of the returned object in order to create a new instance of this FMU.

\throws std::runtime_error if `fmu` does not refer to a valid FMU.
*/
std::unique_ptr<dsb::domain::ISlaveType> MakeSlaveType(
    const boost::filesystem::path& fmu,
    SlaveStarter slaveStarterFunction);


/**
\brief  Makes a new slave instance based on an FMU.

The files in the FMU will be unpacked to a temporary directory, which will
be automatically deleted again when the returned object is destroyed.

\note
As a *temporary* measure until the DSB supports proper observers, an output
stream may optionally be provided for logging of variable values in CSV
format.  If so, a header line containing variable names is printed before this
function returns, and variable values are printed on consecutive lines every
time the returned object's dsb::execution::ISlaveInstance::DoStep() method is
called.

\warning
CSV output is a temporary feature which will be removed in the future, so
future-proof code should omit the `outputStream` parameter.

\param [in] fmu           The FMU file path.
\param [in] outputStream  An output stream to which CSV output will be written.

\throws std::runtime_error if `fmu` does not refer to a valid FMU.
*/
std::unique_ptr<dsb::execution::ISlaveInstance> MakeSlaveInstance(
    const boost::filesystem::path& fmu,
    std::ostream* outputStream = nullptr); // TODO: Temporary; remove when we have proper observers.


}}
#endif  // header guard
