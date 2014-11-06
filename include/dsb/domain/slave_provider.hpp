/**
\file
\brief Slave provider functionality.
*/
#ifndef DSB_DOMAIN_SLAVE_PROVIDER_HPP
#define DSB_DOMAIN_SLAVE_PROVIDER_HPP

#include <string>
#include "boost/noncopyable.hpp"
#include "dsb/model/slave.hpp"
#include "dsb/model/variable.hpp"


namespace dsb
{
namespace domain
{


/// An interface for classes that represent slave types.
class ISlaveType : boost::noncopyable
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
    virtual dsb::model::Variable Variable(size_t index) const = 0;

    /**
    \brief  Creates a new instance of this slave type and connects it to
            the specified execution.

    This function must report whether a slave was successfully instantiated,
    but not necessarily whether it successfully connected to the execution.
    For example, the slave may represent a particular piece of hardware (e.g.
    a human interface device), of which there is only one.  The function would
    then return `false` if multiple instantiations are attempted.

    If the function returns `false`, InstantiationFailureDescription() must
    return a textual description of the reasons for this.

    \returns `true` if a slave was successfully instantiated, `false` otherwise.
    */
    virtual bool InstantiateAndConnect(
        dsb::model::SlaveID slaveID
        /* TODO: Execution locator */) = 0;

    /**
    \brief  A textual description of why a previous InstantiateAndConnect()
            call failed.

    This function is only called if InstantiateAndConnect() has returned
    `false`.
    */
    virtual std::string InstantiationFailureDescription() const = 0;

    // Virtual destructor to allow deletion through base class reference.
    virtual ~ISlaveType() { }
};


void SlaveProvider(
    const std::string& reportEndpoint,
    const std::string& infoEndpoint,
    dsb::domain::ISlaveType& slaveType);


}}      // namespace
#endif  // header guard
