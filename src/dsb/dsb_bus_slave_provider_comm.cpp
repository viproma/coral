#include "dsb/bus/slave_provider_comm.hpp"

#include <cassert>
#include <utility>
#include <vector>

#include "boost/numeric/conversion/cast.hpp"

#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/protocol/glue.hpp"
#include "domain.pb.h"


namespace dsb
{
namespace bus
{

namespace
{
    const char* PROTOCOL_IDENTIFIER = "DSSPI";
    const std::uint16_t PROTOCOL_VERSION = 0;
    const std::string GET_SLAVE_TYPES_REQUEST = "GET_SLAVE_TYPES";
    const std::string INSTANTIATE_SLAVE_REQUEST = "INSTANTIATE_SLAVE";
    const std::string OK_REPLY = "OK";
    const std::string ERROR_REPLY = "ERROR";

    std::vector<dsb::model::SlaveTypeDescription> FromProto(
        const dsbproto::domain::SlaveTypeList& pbSlaves)
    {
        std::vector<dsb::model::SlaveTypeDescription> ret;
        for (const auto& st : pbSlaves.slave_type()) {
            ret.push_back(dsb::protocol::FromProto(st.description()));
        }
        return ret;
    }
}


// =============================================================================
// SlaveProviderClient
// =============================================================================

namespace
{
    // If the slave endpoints have "*" as their address, it means that
    // the slave is listening on the same interface(s) as the slave provider.
    // This helper function takes care of replacing the address if necessary.
    dsb::net::Endpoint MakeSlaveEndpoint(
        const std::string& slaveProviderAddress,
        const std::string& slaveEndpointURL)
    {
        const auto ep = dsb::net::Endpoint{slaveEndpointURL};
        if (ep.Transport() == "tcp") {
            auto inEp = dsb::net::InetEndpoint{ep.Address()};
            if (inEp.Address().IsAnyAddress()) {
                inEp.SetAddress(slaveProviderAddress);
            }
            return inEp.ToEndpoint("tcp");
        } else {
            return ep;
        }
    }
}


using namespace std::placeholders;

class SlaveProviderClient::Private
{
public:
    Private(
        dsb::net::Reactor& reactor,
        const dsb::net::InetEndpoint& endpoint)
        : m_address(endpoint.Address().ToString())
        , m_client{reactor, PROTOCOL_IDENTIFIER, endpoint.ToEndpoint("tcp")}
    {
    }

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    void GetSlaveTypes(
        GetSlaveTypesHandler onComplete,
        std::chrono::milliseconds timeout)
    {
        DSB_INPUT_CHECK(onComplete != nullptr);
        DSB_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
        if (m_slaveTypesCached) {
            onComplete(std::error_code(), m_slaveTypes.data(), m_slaveTypes.size());
        } else {
            m_client.Request(
                PROTOCOL_VERSION,
                GET_SLAVE_TYPES_REQUEST.data(), GET_SLAVE_TYPES_REQUEST.size(),
                nullptr, 0,
                timeout,
                std::bind(
                    &Private::OnGetSlaveTypesReply, this,
                    std::move(onComplete), _1, _2, _3, _4, _5));
        }
    }

    void InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds instantiationTimeout,
        InstantiateSlaveHandler onComplete,
        std::chrono::milliseconds requestTimeout)
    {
        if (requestTimeout == std::chrono::milliseconds(0)) {
            requestTimeout = 2 * instantiationTimeout;
        }
        DSB_INPUT_CHECK(instantiationTimeout > std::chrono::milliseconds(0));
        DSB_INPUT_CHECK(onComplete != nullptr);
        DSB_INPUT_CHECK(requestTimeout > instantiationTimeout);
        dsbproto::domain::InstantiateSlaveData args;
        args.set_slave_type_uuid(slaveTypeUUID);
        args.set_timeout_ms(boost::numeric_cast<google::protobuf::int32>(
            instantiationTimeout.count()));
        const auto body = args.SerializeAsString();
        assert(!body.empty());
        m_client.Request(
            PROTOCOL_VERSION,
            INSTANTIATE_SLAVE_REQUEST.data(),
            INSTANTIATE_SLAVE_REQUEST.size(),
            body.data(),
            body.size(),
            requestTimeout,
            std::bind(
                &Private::OnInstantiateSlaveReply, this,
                std::move(onComplete), _1, _2, _3, _4, _5));
    }

private:
    void OnGetSlaveTypesReply(
        GetSlaveTypesHandler completionHandler,
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        if (ec) {
            completionHandler(ec, nullptr, 0);
            return;
        }
        const auto reply = std::string(replyHeader, replyHeaderSize);
        if (reply == OK_REPLY) {
            dsbproto::domain::SlaveTypeList slaveTypeList;
            if (slaveTypeList.ParseFromArray(replyBody, replyBodySize)) {
                auto slaveTypes = FromProto(slaveTypeList);
                m_slaveTypesCached = true; // TODO: Add "expiry date"?
                completionHandler(
                    std::error_code{},
                    slaveTypes.data(),
                    slaveTypes.size());
            } else {
                completionHandler(
                    make_error_code(std::errc::bad_message),
                    nullptr, 0);
            }
        } else {
            completionHandler(
                make_error_code(std::errc::bad_message),
                nullptr, 0);
        }
    }

    void OnInstantiateSlaveReply(
        InstantiateSlaveHandler completionHandler,
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        if (ec) {
            completionHandler(ec, dsb::net::SlaveLocator{}, std::string{});
            return;
        }
        const auto reply = std::string{replyHeader, replyHeaderSize};
        if (reply == OK_REPLY) {
            dsbproto::domain::InstantiateSlaveReply replyData;
            if (replyData.ParseFromArray(replyBody, replyBodySize)) {
                // Trandlate "*" in the slave addresses with the slave
                // provider address.
                const auto slaveLocator = dsb::net::SlaveLocator{
                    MakeSlaveEndpoint(
                        m_address,
                        replyData.slave_locator().control_endpoint()),
                    MakeSlaveEndpoint(
                        m_address,
                        replyData.slave_locator().data_pub_endpoint())
                };

                completionHandler(
                    std::error_code{},
                    slaveLocator,
                    std::string{});
                return;
            } // else fall through to the end of the function
        } else if (reply == ERROR_REPLY) {
            completionHandler(
                make_error_code(dsb::error::generic_error::operation_failed),
                dsb::net::SlaveLocator{},
                std::string{replyBody, replyBodySize});
            return;
        }
        // If we get here, it means we have received bad data.
        completionHandler(
            make_error_code(std::errc::bad_message),
            dsb::net::SlaveLocator{},
            std::string{});
    }

    const std::string m_address;
    dsb::protocol::RRClient m_client;
    bool m_slaveTypesCached = false;
    std::vector<dsb::model::SlaveTypeDescription> m_slaveTypes;
};


SlaveProviderClient::SlaveProviderClient(
    dsb::net::Reactor& reactor,
    const dsb::net::InetEndpoint& endpoint)
    : m_private(std::make_unique<Private>(reactor, endpoint))
{
}


SlaveProviderClient::~SlaveProviderClient() DSB_NOEXCEPT
{
    // Do nothing, it's all handled by ~Private().
}


SlaveProviderClient::SlaveProviderClient(SlaveProviderClient&& other)
    DSB_NOEXCEPT
    : m_private(std::move(other.m_private))
{
}


SlaveProviderClient& SlaveProviderClient::operator=(SlaveProviderClient&& other)
    DSB_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}


void SlaveProviderClient::GetSlaveTypes(
    GetSlaveTypesHandler onComplete,
    std::chrono::milliseconds timeout)
{
    m_private->GetSlaveTypes(onComplete, timeout);
}


void SlaveProviderClient::InstantiateSlave(
    const std::string& slaveTypeUUID,
    std::chrono::milliseconds instantiationTimeout,
    InstantiateSlaveHandler onComplete,
    std::chrono::milliseconds requestTimeout)
{
    m_private->InstantiateSlave(
        slaveTypeUUID,
        instantiationTimeout,
        onComplete,
        requestTimeout == std::chrono::milliseconds(0)
            ? 2*instantiationTimeout
            : requestTimeout);
}


// =============================================================================
// SlaveProviderServerHandler
// =============================================================================

// NOTE: This class doesn't really need to be pimpl'd since it's all in the
//       same .cpp file, but it I had it in the .hpp file at first, so it made
//       sense then.  Keeping it as it is for the time being, in case I want to
//       move it back to the header.
// TODO: Consider un-pimpl-ing it.
class SlaveProviderServerHandler : public dsb::protocol::RRServerProtocolHandler
{
public:
    SlaveProviderServerHandler(std::shared_ptr<SlaveProviderOps> slaveProvider);

    ~SlaveProviderServerHandler() DSB_NOEXCEPT;

    SlaveProviderServerHandler(const SlaveProviderServerHandler&) = delete;
    SlaveProviderServerHandler& operator=(const SlaveProviderServerHandler&) = delete;

    SlaveProviderServerHandler(SlaveProviderServerHandler&&) DSB_NOEXCEPT;
    SlaveProviderServerHandler& operator=(SlaveProviderServerHandler&&) DSB_NOEXCEPT;

    bool HandleRequest(
        const std::string& protocolIdentifier,
        std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize);

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


class SlaveProviderServerHandler::Private
{
public:
    Private(std::shared_ptr<SlaveProviderOps> slaveProvider)
        : m_slaveProvider{slaveProvider}
    {
    }

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    bool HandleRequest(
        const std::string& protocolIdentifier,
        std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize)
    {
        assert(protocolIdentifier == PROTOCOL_IDENTIFIER);
        assert(protocolVersion == PROTOCOL_VERSION);
        const auto request = std::string{requestHeader, requestHeaderSize};
        if (request == GET_SLAVE_TYPES_REQUEST) {
            return HandleGetSlaveTypesRequest(
                requestBody, requestBodySize,
                replyHeader, replyHeaderSize,
                replyBody, replyBodySize);
        } else if (request == INSTANTIATE_SLAVE_REQUEST) {
            return HandleInstantiateSlaveRequest(
                requestBody, requestBodySize,
                replyHeader, replyHeaderSize,
                replyBody, replyBodySize);
        } else {
            DSB_LOG_TRACE("SlaveProviderServerHandler: Ignoring request due to invalid request header");
            return false;
        }
    }

private:
    bool HandleGetSlaveTypesRequest(
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize)
    {
        if (requestBody != nullptr) {
            DSB_LOG_TRACE("SlaveProviderServerHandler: Ignoring request due to unexpected request body");
            return false;
        }
        dsbproto::domain::SlaveTypeList slaveTypeList;
        const int n = m_slaveProvider->GetSlaveTypeCount();
        for (int i = 0; i < n; ++i) {
            *(slaveTypeList.add_slave_type()->mutable_description()) =
                dsb::protocol::ToProto(m_slaveProvider->GetSlaveType(i));
        }
        m_replyBodyBuffer = slaveTypeList.SerializeAsString();

        replyHeader = OK_REPLY.data();
        replyHeaderSize = OK_REPLY.size();
        replyBody = m_replyBodyBuffer.data();
        replyBodySize = m_replyBodyBuffer.size();
        return true;
    }

    bool HandleInstantiateSlaveRequest(
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize)
    {
        if (requestBody == nullptr) {
            DSB_LOG_TRACE("SlaveProviderServerHandler: Ignoring request due to missing request body");
            return false;
        }
        dsbproto::domain::InstantiateSlaveData args;
        if (!args.ParseFromArray(requestBody, requestBodySize)) {
            DSB_LOG_TRACE("SlaveProviderServerHandler: Ignoring request due to malformed request body");
            return false;
        }
        try {
            const auto slaveLocator = m_slaveProvider->InstantiateSlave(
                args.slave_type_uuid(),
                std::chrono::milliseconds(args.timeout_ms()));
            replyHeader = OK_REPLY.data();
            replyHeaderSize = OK_REPLY.size();
            dsbproto::domain::InstantiateSlaveReply data;
            data.mutable_slave_locator()->set_control_endpoint(
                slaveLocator.ControlEndpoint().URL());
            data.mutable_slave_locator()->set_data_pub_endpoint(
                slaveLocator.DataPubEndpoint().URL());
            m_replyBodyBuffer = data.SerializeAsString();
        } catch (const std::runtime_error& e) {
             replyHeader = ERROR_REPLY.data();
             replyHeaderSize = ERROR_REPLY.size();
             m_replyBodyBuffer = e.what();
        }
        assert(replyHeader != nullptr);
        assert(replyHeaderSize > 0);
        assert(!m_replyBodyBuffer.empty());
        replyBody = m_replyBodyBuffer.data();
        replyBodySize = m_replyBodyBuffer.size();
        return true;
    }

    std::shared_ptr<SlaveProviderOps> m_slaveProvider;
    std::string m_replyBodyBuffer;
};


SlaveProviderServerHandler::SlaveProviderServerHandler(
    std::shared_ptr<SlaveProviderOps> slaveProvider)
    : m_private(std::make_unique<Private>(slaveProvider))
{
}


SlaveProviderServerHandler::~SlaveProviderServerHandler() DSB_NOEXCEPT
{
    // Do nothing, it's all handled by ~Private().
}


SlaveProviderServerHandler::SlaveProviderServerHandler(SlaveProviderServerHandler&& other)
    DSB_NOEXCEPT
    : m_private(std::move(other.m_private))
{
}


SlaveProviderServerHandler& SlaveProviderServerHandler::operator=(SlaveProviderServerHandler&& other)
    DSB_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}


bool SlaveProviderServerHandler::HandleRequest(
    const std::string& protocolIdentifier,
    std::uint16_t protocolVersion,
    const char* requestHeader, size_t requestHeaderSize,
    const char* requestBody, size_t requestBodySize,
    const char*& replyHeader, size_t& replyHeaderSize,
    const char*& replyBody, size_t& replyBodySize)
{
    return m_private->HandleRequest(
        protocolIdentifier,
        protocolVersion,
        requestHeader, requestHeaderSize,
        requestBody, requestBodySize,
        replyHeader, replyHeaderSize,
        replyBody, replyBodySize);
}


// =============================================================================
// MakeSlaveProviderServer
// =============================================================================

void MakeSlaveProviderServer(
    dsb::protocol::RRServer& server,
    std::shared_ptr<SlaveProviderOps> slaveProvider)
{
    server.AddProtocolHandler(
        PROTOCOL_IDENTIFIER,
        PROTOCOL_VERSION,
        std::make_shared<SlaveProviderServerHandler>(slaveProvider));
}


}} // namespace
