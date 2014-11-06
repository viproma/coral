#include <cassert>
#include <deque>
#include <exception>
#include <iostream>
#include <string>

#include "boost/chrono.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/thread.hpp"
#include "zmq.hpp"
#include "fmilib.h"

#include "fmilibcpp/fmi1/Fmu.hpp"
#include "fmilibcpp/Fmu.hpp"
#include "fmilibcpp/ImportContext.hpp"

#include "dsb/comm.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/domain.hpp"
#include "dsb/slave_provider.hpp"
#include "dsb/util.hpp"

#include "domain.pb.h"


namespace dp = dsb::protocol::domain;


void DsbToProto(
    const dsb::model::Variable& dsbVariable,
    dsbproto::variable::VariableDefinition& protoVariable)
{
    protoVariable.set_id(dsbVariable.ID());
    protoVariable.set_name(dsbVariable.Name());
    switch (dsbVariable.DataType()) {
        case dsb::model::REAL_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::REAL);
            break;
        case dsb::model::INTEGER_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::INTEGER);
            break;
        case dsb::model::BOOLEAN_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::BOOLEAN);
            break;
        case dsb::model::STRING_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::STRING);
            break;
        default:
            assert (!"Unknown data type");
    }
    switch (dsbVariable.Causality()) {
        case dsb::model::PARAMETER_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::PARAMETER);
            break;
        case dsb::model::CALCULATED_PARAMETER_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::CALCULATED_PARAMETER);
            break;
        case dsb::model::INPUT_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::INPUT);
            break;
        case dsb::model::OUTPUT_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::OUTPUT);
            break;
        case dsb::model::LOCAL_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::LOCAL);
            break;
        default:
            assert (!"Unknown causality");
    }
    switch (dsbVariable.Variability()) {
        case dsb::model::CONSTANT_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::CONSTANT);
            break;
        case dsb::model::FIXED_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::FIXED);
            break;
        case dsb::model::TUNABLE_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::TUNABLE);
            break;
        case dsb::model::DISCRETE_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::DISCRETE);
            break;
        case dsb::model::CONTINUOUS_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::CONTINUOUS);
            break;
        default:
            assert (!"Unknown variability");
    }
}


void SlaveProvider(
    const std::string& reportEndpoint,
    const std::string& infoEndpoint,
    dsb::slave::ISlaveType& slaveType)
{
    auto context = zmq::context_t();
    auto report = zmq::socket_t(context, ZMQ_PUB);
    report.connect(reportEndpoint.c_str());

    const auto myId = dsb::util::RandomUUID();

    auto info = zmq::socket_t(context, ZMQ_DEALER);
    info.setsockopt(ZMQ_IDENTITY, myId.data(), myId.size());
    info.connect(infoEndpoint.c_str());

    namespace dp = dsb::protocol::domain;
    zmq::pollitem_t pollItem = { info, 0, ZMQ_POLLIN, 0 };

    namespace bc = boost::chrono;
    const auto HELLO_INTERVAL = bc::milliseconds(1000);
    auto nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
    for (;;) {
        const auto timeout = bc::duration_cast<bc::milliseconds>
                             (nextHelloTime - bc::steady_clock::now());
        zmq::poll(&pollItem, 1, boost::numeric_cast<long>(timeout.count()));
        if (pollItem.revents & ZMQ_POLLIN) {
            std::deque<zmq::message_t> msg;
            dsb::comm::Receive(info, msg);
            if (msg.size() < 4 || msg[0].size() > 0 || msg[2].size() > 0) {
                throw dsb::error::ProtocolViolationException("Wrong message format");
            }
            const auto header = dp::ParseHeader(msg[3]);
            switch (header.messageType) {
                case dp::MSG_GET_SLAVE_LIST: {
                    msg[3] = dp::CreateHeader(dp::MSG_SLAVE_LIST, header.protocol);
                    dsbproto::domain::SlaveTypeList stl;
                    auto st = stl.add_slave_type();
                    st->set_name(slaveType.Name());
                    st->set_uuid(slaveType.Uuid());
                    st->set_description(slaveType.Description());
                    st->set_author(slaveType.Author());
                    st->set_version(slaveType.Version());
                    for (size_t i = 0; i < slaveType.VariableCount(); ++i) {
                        auto v = st->add_variable();
                        DsbToProto(slaveType.Variable(i), *v);
                    }
                    msg.push_back(zmq::message_t());
                    dsb::protobuf::SerializeToFrame(stl, msg.back());
                    break; }
                default:
                    assert (false);
            }
            dsb::comm::Send(info, msg);
        }

        if (bc::steady_clock::now() >= nextHelloTime) {
            std::deque<zmq::message_t> msg;
            msg.push_back(dp::CreateHeader(dp::MSG_SLAVEPROVIDER_HELLO,
                                           dp::MAX_PROTOCOL_VERSION));
            msg.push_back(dsb::comm::ToFrame(myId));
            dsb::comm::Send(report, msg);
            nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
        }
    }
}


class FmiSlaveType : public dsb::slave::ISlaveType
{
public:
    FmiSlaveType(
        const std::string& fmuPath,
        const std::string& slaveExePath);
    ~FmiSlaveType();

    std::string Name() const override;
    std::string Uuid() const override;
    std::string Description() const override;
    std::string Author() const override;
    std::string Version() const override;
    size_t VariableCount() const override;
    virtual dsb::model::Variable Variable(size_t index) const override;
    virtual bool InstantiateAndConnect(dsb::model::SlaveID slaveID /* TODO: Execution locator */) override;
    virtual std::string InstantiationFailureDescription() const override;

private:
    const std::string m_slaveExePath;
    dsb::util::TempDir m_unzipDir;
    std::shared_ptr<fmilib::fmi1::Fmu> m_fmu;
    fmi1_import_variable_list_t* m_varList;
};


FmiSlaveType::FmiSlaveType(
    const std::string& fmuPath,
    const std::string& slaveExePath)
    : m_slaveExePath(slaveExePath), m_varList(nullptr)
{
    auto ctx = fmilib::MakeImportContext();
    auto fmu = ctx->Import(fmuPath, m_unzipDir.Path().string());
    if (fmu->FmiVersion() != fmilib::kFmiVersion1_0) {
        throw std::runtime_error("Only FMI version 1.0 supported");
    }
    m_fmu = std::static_pointer_cast<fmilib::fmi1::Fmu>(fmu);
    m_varList = fmi1_import_get_variable_list(m_fmu->Handle());
}

FmiSlaveType::~FmiSlaveType()
{
    assert (m_varList);
    fmi1_import_free_variable_list(m_varList);
}

std::string FmiSlaveType::Name() const 
{
    return m_fmu->ModelName();
}

std::string FmiSlaveType::Uuid() const 
{
    return m_fmu->GUID();
}

std::string FmiSlaveType::Description() const 
{
    return m_fmu->Description();
}

std::string FmiSlaveType::Author() const 
{
    return m_fmu->Author();
}

std::string FmiSlaveType::Version() const 
{
    return m_fmu->ModelVersion();
}

size_t FmiSlaveType::VariableCount() const 
{
    return fmi1_import_get_variable_list_size(m_varList);
}

dsb::model::Variable FmiSlaveType::Variable(size_t index) const 
{
    auto fmiVar = fmi1_import_get_variable(m_varList, index);

    dsb::model::DataType dataType;
    switch (fmi1_import_get_variable_base_type(fmiVar)) {
        case fmi1_base_type_real:
            dataType = dsb::model::REAL_DATATYPE;
            break;
        case fmi1_base_type_int:
            dataType = dsb::model::INTEGER_DATATYPE;
            break;
        case fmi1_base_type_bool:
            dataType = dsb::model::BOOLEAN_DATATYPE;
            break;
        case fmi1_base_type_str:
            dataType = dsb::model::STRING_DATATYPE;
            break;
        case fmi1_base_type_enum:
            assert (!"Enum types not supported yet");
            break;
        default:
            assert (!"Unknown type");
    }

    dsb::model::Variability variability;
    switch (fmi1_import_get_variability(fmiVar)) {
        case fmi1_variability_enu_constant:
            variability = dsb::model::CONSTANT_VARIABILITY;
            break;
        case fmi1_variability_enu_parameter:
            variability = dsb::model::FIXED_VARIABILITY;
            break;
        case fmi1_variability_enu_discrete:
            variability = dsb::model::DISCRETE_VARIABILITY;
            break;
        case fmi1_variability_enu_continuous:
            variability = dsb::model::CONTINUOUS_VARIABILITY;
            break;
        default:
            assert (!"Variable with variability 'unknown' encountered");
    }

    dsb::model::Causality causality;
    switch (fmi1_import_get_causality(fmiVar)) {
        case fmi1_causality_enu_input:
            causality = (variability == dsb::model::FIXED_VARIABILITY)
                        ? dsb::model::PARAMETER_CAUSALITY
                        : dsb::model::INPUT_CAUSALITY;
            break;
        case fmi1_causality_enu_output:
            causality = dsb::model::OUTPUT_CAUSALITY;
            break;
        case fmi1_causality_enu_internal:
            causality = dsb::model::LOCAL_CAUSALITY;
            break;
        default:
            assert (!"Variable with causality 'none' encountered");
    }

    return dsb::model::Variable(
        index,
        fmi1_import_get_variable_name(fmiVar),
        dataType,
        causality,
        variability);
}

bool FmiSlaveType::InstantiateAndConnect(dsb::model::SlaveID slaveID  /* TODO: Execution locator */ )
{
    std::vector<std::string> args;
    args.push_back(boost::lexical_cast<std::string>(slaveID));
    dsb::util::SpawnProcess(m_slaveExePath, args);
    return false;
}

std::string FmiSlaveType::InstantiationFailureDescription() const 
{
    assert (!"InstantiationFailureDescription() not implemented yet");
    return std::string();
}


int main(int argc, const char** argv)
{
try {
    if (argc < 4) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Usage: " << self << " <report> <info> <fmu path>\n"
                  << "  report   = Slave provider report endpoint (e.g. tcp://myhost:5432)\n"
                  << "  info     = Slave provider info endpoint (e.g. tcp://myhost:5432)\n"
                  << "  fmu path = Path to FMI1 FMU"
                  << std::endl;
        return 0;
    }
    const auto reportEndpoint = std::string(argv[1]);
    const auto infoEndpoint = std::string(argv[2]);
    const auto fmuPath = std::string(argv[3]);

    FmiSlaveType fmu(fmuPath);
    SlaveProvider(reportEndpoint, infoEndpoint, fmu);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
return 0;
}


