#ifndef DSB_LIBRARY_HPP
#define DSB_LIBRARY_HPP

#include <memory>
#include <string>
#include <vector>
#include "dsb/sequence.hpp"


namespace dsb
{
namespace library
{


/**
\brief  Slave variable data types.

These correspond to the FMI data types.
*/
enum DataType
{
    REAL_DATATYPE       = 1,
    INTEGER_DATATYPE    = 1 << 1,
    BOOLEAN_DATATYPE    = 1 << 2,
    STRING_DATATYPE     = 1 << 3,
    // Reserved: STRUCTURED_DATATYPE = 1 << 4,
};


/**
\brief  Slave variable causalities.

These correspond to the FMI causality definitions.
*/
enum Causality
{
    PARAMETER_CAUSALITY = 1,
    // Reserved: CALCULATED_PARAMETER_CAUSALITY = 1 << 1,
    INPUT_CAUSALITY     = 1 << 2,
    OUTPUT_CAUSALITY    = 1 << 3,
    LOCAL_CAUSALITY     = 1 << 4,
};


/**
\brief  Slave variable variabilities.

These correspond to the FMI variability definitions.
*/
enum Variability
{
    // Reserved: CONSTANT_VARIABILITY = 1,
    FIXED_VARIABILITY       = 1 << 1,
    TUNABLE_VARIABILITY     = 1 << 2,
    DISCRETE_VARIABILITY    = 1 << 3,
    CONTINUOUS_VARIABILITY  = 1 << 4,
};


/// The properties of one of a slave's variables.
struct VariableInfo
{
    VariableInfo(
        int reference_,
        const std::string& name_,
        DataType dataType_,
        Causality causality_,
        Variability variability_)
        : reference(reference_),
          name(name_),
          dataType(dataType_),
          causality(causality_),
          variability(variability_)
    { }

    /// The variable reference number.
    int reference;

    /// The variable name.
    std::string name;

    /// The data type.
    DataType dataType;

    /// The causality.
    Causality causality;

    /// The variability.
    Variability variability;
};


/// Properties common to all slaves of a certain type.
class SlaveType
{
public:
    /**
    \brief  Constructor.

    \param [in] name            A name that uniquely identifies the slave type.
    \param [in] beginVariables  An iterator that points to the start of a list
                                of VariableInfo structures that describe the
                                variables of the slave.
    \param [in] endVariables    An iterator that points to the end of a list
                                of VariableInfo structures that describe the
                                variables of the slave.
    */
    template<typename It>
    SlaveType(const std::string& name, It beginVariables, It endVariables)
        : m_name(name), m_variables(beginVariables, endVariables) { }

    /// The slave type name.
    const std::string& Name() const;

    /// The slave's visible variables.
    dsb::sequence::Sequence<const VariableInfo&> Variables() const;

    /**
    \brief Returns information about the named variable.
    \throws std::out_of_range if there is no variable with the given name.
    */
    const VariableInfo& Variable(const std::string& name) const;

private:
    std::string m_name;
    std::vector<VariableInfo> m_variables;
};


/**
\brief  A class that contains the addresses of ZMQ endpoints used for a specific
        execution.
*/
class ExecutionLocator
{
public:
    /**
    \brief  The ZMQ endpoint that should be used by participants that
            connect to the execution controller over a network.
    */
    const char* NetworkEndpoint();

    /**
    \brief  The ZMQ endpoint that should be used by participants that
            are on the same machine as the execution controller, but in
            separate processes.
    */
    const char* InterProcessEndpoint();

    /**
    \brief  The ZMQ endpoint that should be used by participants that
            run in the same process as the execution controller.
    */
    const char* InterThreadEndpoint();
};


/**
\brief  A base class that defines a generic interface for retrieving information
        about the available slave types and for instantiating slaves based on
        these types.
*/
class Library
{
public:
    /// Returns the available slave types.
    virtual dsb::sequence::Sequence<const SlaveType&> SlaveTypes() = 0;

    /**
    \brief  Returns a pointer to a SlaveType object corresponding to the given
            type name, or null if the slave type is unknown.

    The default implementation of this method simply performs a linear search
    through the types returned by SlaveTypes(), and if it finds one with a
    matching name, it returns a pointer to it.  A subclass may override the
    method if it is able to resolve slave type name with less than O(n)
    complexity.

    The library retains ownership of the returned object.

    \throws std::out_of_range if there is no slave type with the given name.
    */
    virtual const SlaveType* FindSlaveType(const std::string& slaveTypeName);

    /**
    \brief  Requests that a slave be instantiated and connected to an execution.

    \param [in] type        The type of slave that should be instantiated.
                            This must be an object acquired from the present
                            Library instance.
    \param [in] name        A unique name that will be used to identify the
                            slave in the context of the execution.
    \param [in] execution   A locator for the execution to which the slave
                            is requested to connect.

    \returns `false` if the request failed because the slave has reached
        its maximum number of instantiations, and `true` otherwise.  Note
        that a `true` return value does *not* mean that instantiation
        and/or connection necessarily has suceeded, or that it will succeed,
        only that it didn't fail due to a hard limit on the number of
        instantiations.
    */
    virtual bool RequestInstantiation(
        const SlaveType& type,
        const std::string& name,
        const ExecutionLocator& execution) = 0;

    // Allows deletion through base class pointer.
    virtual ~Library() { }
};


}}      // namespace
#endif  // header guard
