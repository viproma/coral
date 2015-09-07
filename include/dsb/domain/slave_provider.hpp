/**
\file
\brief Slave provider functionality.
*/
#ifndef DSB_DOMAIN_SLAVE_PROVIDER_HPP
#define DSB_DOMAIN_SLAVE_PROVIDER_HPP

#include <string>
#include <vector>
#include "boost/noncopyable.hpp"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


namespace dsb
{
namespace domain
{


/// An interface for classes that represent slave types.
class ISlaveType : boost::noncopyable
// TODO: Rename this to ISlaveFactory or similar?
{
public:
    /// The name of this slave type.
    virtual std::string Name() const = 0;

    /// A UUID for this slave type.
    virtual std::string Uuid() const = 0;

    /// A description of this slave type.
    virtual std::string Description() const = 0;

    /// The author.
    virtual std::string Author() const = 0;

    /// The version of this particular slave type.
    virtual std::string Version() const = 0;

    /// How many variables slaves of this type have.
    virtual size_t VariableCount() const = 0;

    /**
    \brief  Information about the `index`-th variable.

    Each variable must have a unique ID (which may, but is not required to, be
    equal to `index`) and a unique name in the context of this slave type.
    */
    virtual dsb::model::VariableDescription Variable(size_t index) const = 0;

    /**
    \brief  Creates a new instance of this slave type.

    This function must report whether a slave was successfully instantiated.
    For example, the slave may represent a particular piece of hardware (e.g.
    a human interface device), of which there is only one.  The function would
    then return `false` if multiple instantiations are attempted.

    If the function returns `true`, it must also update `slaveLocator` with
    information about the new slave.  `slaveLocator.Endpoint()` may then have
    one of three forms:

      1. "Normal", i.e. `transport://address`
      2. Empty, which means that the slave is accessible through the same
         endpoint as the slave provider (typically a proxy), except of course
         with a different identity.
      3. Only a port specification starting with a colon, e.g. `:12345`.
         This may be used if the slave provider is bound to a TCP endpoint,
         and the slave is accessible on the same hostname but with a different
         port number.

    If the function returns `false`, InstantiationFailureDescription() must
    return a textual description of the reasons for this.  `slaveLocator` must
    then be left untouched.

    \param [out] slaveLocator
        An object that describes how to connect to the slave.  See the list
        above for different endpoint formats.

    \returns `true` if a slave was successfully instantiated, `false` otherwise.
    */
    virtual bool Instantiate(dsb::net::SlaveLocator& slaveLocator) = 0;

    /**
    \brief  A textual description of why a previous Instantiate() call failed.

    This function is only called if Instantiate() has returned `false`.
    */
    virtual std::string InstantiationFailureDescription() const = 0;

    // Virtual destructor to allow deletion through base class reference.
    virtual ~ISlaveType() { }
};


void SlaveProvider(
    const dsb::net::DomainLocator& domainLocator,
    const std::vector<dsb::domain::ISlaveType*>& slaveTypes); // TODO: Range API here.


}}      // namespace
#endif  // header guard
