#ifndef DSB_BUS_SLAVE_AGENT_HPP
#define DSB_BUS_SLAVE_AGENT_HPP

#include <cstdint>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "zmq.hpp"
#include "control.pb.h"


namespace dsb
{
namespace bus
{

// =============================================================================
// TEMPORARY placeholders for FMI-based interface
enum DataType
{
    REAL_DATATYPE       = 1,
    INTEGER_DATATYPE    = 1 << 1,
    BOOLEAN_DATATYPE    = 1 << 2,
    STRING_DATATYPE     = 1 << 3,
    // Reserved: STRUCTURED_DATATYPE = 1 << 4,
};

enum Causality
{
    PARAMETER_CAUSALITY = 1,
    // Reserved: CALCULATED_PARAMETER_CAUSALITY = 1 << 1,
    INPUT_CAUSALITY     = 1 << 2,
    OUTPUT_CAUSALITY    = 1 << 3,
    LOCAL_CAUSALITY     = 1 << 4,
};

enum Variability
{
    CONSTANT_VARIABILITY    = 1,
    FIXED_VARIABILITY       = 1 << 1,
    TUNABLE_VARIABILITY     = 1 << 2,
    DISCRETE_VARIABILITY    = 1 << 3,
    CONTINUOUS_VARIABILITY  = 1 << 4,
};

struct VariableInfo
{
    VariableInfo(unsigned reference_, const std::string& name_, DataType dataType_, Causality causality_, Variability variability_)
        : reference(reference_), name(name_), dataType(dataType_), causality(causality_), variability(variability_)
    { }

    unsigned reference;
    std::string name;
    DataType dataType;
    Causality causality;
    Variability variability;
};

class ISlaveInstance
{
public:
    virtual void Setup(double startTime, double stopTime) = 0;
    virtual std::vector<VariableInfo> Variables() = 0;
    virtual double GetRealVariable(unsigned varRef) = 0;
    virtual int GetIntegerVariable(unsigned varRef) = 0;
    virtual bool GetBooleanVariable(unsigned varRef) = 0;
    virtual std::string GetStringVariable(unsigned varRef) = 0;
    virtual void SetRealVariable(unsigned varRef, double value) = 0;
    virtual void SetIntegerVariable(unsigned varRef, int value) = 0;
    virtual void SetBooleanVariable(unsigned varRef, bool value) = 0;
    virtual void SetStringVariable(unsigned varRef, const std::string& value) = 0;
    virtual bool DoStep(double currentT, double deltaT) = 0;
};
// =============================================================================


/**
\brief  A class which contains the state of the slave and takes care of
        responding to requests from the master node in an appropriate manner.
*/
class SlaveAgent
{
public:
    /**
    \brief  Constructs a new SlaveAgent.

    \param [in] id              The slave ID.
    \param [in] dataSub         A SUB socket to be used for receiving variables.
    \param [in] dataPub         A PUB socket to be used for sending variables.
    \param [in] slaveInstance   (Temporary) A pointer to the object which
                                contains the slave's mathematical model.
    */
    SlaveAgent(
        uint16_t id,
        zmq::socket_t dataSub,
        zmq::socket_t dataPub,
        std::unique_ptr<ISlaveInstance> slaveInstance
        );

    /**
    \brief  Prepares the first message (HELLO) which is to be sent to the master
            and stores it in `msg`.
    */
    void Start(std::deque<zmq::message_t>& msg);

    /**
    \brief  Responds to a message from the master.
    
    On input, `msg` must be the message received from master, and on output,
    it will contain the slave's reply.  Internally, the function forwards to
    the handler function that corresponds to the slave's current state.
    */
    void RequestReply(std::deque<zmq::message_t>& msg);

private:
    // Each of these functions correspond to one of the slave's possible states.
    // On input, `msg` is a message from the master node, and when the function
    // returns, `msg` must contain the reply.  If the message triggers a state
    // change, the handler function must update m_stateHandler to point to the
    // function for the new state.
    void ConnectingHandler(std::deque<zmq::message_t>& msg);
    void ConnectedHandler(std::deque<zmq::message_t>& msg);
    void ReadyHandler(std::deque<zmq::message_t>& msg);
    void PublishedHandler(std::deque<zmq::message_t>& msg);
    void StepFailedHandler(std::deque<zmq::message_t>& msg);

    // Performs the "set variables" operation for ReadyHandler(), including
    // filling `msg` with a reply message.
    void HandleSetVars(std::deque<zmq::message_t>& msg);

    // Performs the "connect variables" operation for ReadyHandler(), including
    // filling `msg` with a reply message.
    void HandleConnectVars(std::deque<zmq::message_t>& msg);

    // Performs the time step for ReadyHandler()
    bool Step(const dsbproto::control::StepData& stepData);

    // A pointer to the handler function for the current state.
    void (SlaveAgent::* m_stateHandler)(std::deque<zmq::message_t>&);

    const uint16_t m_id; // The slave's ID number in the current execution
    zmq::socket_t m_dataSub;
    zmq::socket_t m_dataPub;
    std::unique_ptr<ISlaveInstance> m_slaveInstance;
    double m_currentTime;
    double m_lastStepSize;

    struct RemoteVariable
    {
        uint16_t slave;
        uint16_t var;
        bool operator<(const RemoteVariable& other) const {
            if (slave < other.slave) return true;
            else if (slave == other.slave) return var < other.var;
            else return false;
        }
    };
    std::map<RemoteVariable, uint16_t> m_connections;
};


// TODO: move this to compat_helpers.hpp
#if defined(_MSC_VER) && _MSC_VER <= 1800
#   define noexcept
#endif


/// Exception thrown when the slave receives a TERMINATE command.
class Shutdown : std::exception
{
public:
    const char* what() const noexcept override { return "Normal shutdown requested by master"; }
};


}}      // namespace
#endif  // header guard
