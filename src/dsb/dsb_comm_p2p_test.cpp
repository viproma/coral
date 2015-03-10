#include "gtest/gtest.h"

#include <deque>
#include "boost/thread.hpp"
#include "boost/chrono.hpp"
#include "zmq.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/compat_helpers.hpp"

#include "dsb/comm/p2p.hpp"

using namespace dsb::comm;


TEST(dsb_comm, P2PProxy_bidirectional)
{
    const std::string client1Id = "client1";
    const std::string server1Id = "server1";
    const std::string client2Id = "client2";
    const std::string server2Id = "server2";
    const std::string body1 = "wohoo!";
    const std::string body2 = "yeeeah!";
    const std::string body3 = "eeer...";
    const std::string body4 = "yeee-haaw!";

    auto ctx = std::make_shared<zmq::context_t>();
    auto req1 = zmq::socket_t(*ctx, ZMQ_REQ);
    req1.setsockopt(ZMQ_IDENTITY, client1Id.c_str(), client1Id.size());
    auto rep1 = zmq::socket_t(*ctx, ZMQ_DEALER);
    rep1.setsockopt(ZMQ_IDENTITY, server1Id.c_str(), server1Id.size());
    auto req2  = zmq::socket_t(*ctx, ZMQ_REQ);
    req2.setsockopt(ZMQ_IDENTITY, client2Id.c_str(), client2Id.size());
    auto rep2 = zmq::socket_t(*ctx, ZMQ_DEALER);
    rep2.setsockopt(ZMQ_IDENTITY, server2Id.c_str(), server2Id.size());

    std::uint16_t port = 0;
    auto proxy = SpawnP2PProxy(ctx, "*", port);
    ASSERT_GT(port, 0);

    const auto endpoint = "tcp://localhost:" + std::to_string(port);
    req1.connect(endpoint.c_str());
    rep1.connect(endpoint.c_str());
    req2.connect(endpoint.c_str());
    rep2.connect(endpoint.c_str());
    // Give ZeroMQ some time to establish the connections.
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    // Send a request from client 1 to server 2
    std::deque<zmq::message_t> req1Msg;
    req1Msg.push_back(dsb::comm::ToFrame(server2Id));
    req1Msg.push_back(dsb::comm::ToFrame(""));
    req1Msg.push_back(dsb::comm::ToFrame(body1));
    dsb::comm::Send(req1, req1Msg);

    // Send a request from client 2 to server 1
    std::deque<zmq::message_t> req2Msg;
    req2Msg.push_back(dsb::comm::ToFrame(server1Id));
    req2Msg.push_back(dsb::comm::ToFrame(""));
    req2Msg.push_back(dsb::comm::ToFrame(body2));
    dsb::comm::Send(req2, req2Msg);

    // Server 1: Receive request from client 2
    std::deque<zmq::message_t> rep1Msg;
    dsb::comm::Receive(rep1, rep1Msg);
    ASSERT_EQ(4u, rep1Msg.size());
    EXPECT_TRUE(rep1Msg[0].size() == 0);
    EXPECT_EQ(client2Id, dsb::comm::ToString(rep1Msg[1]));
    EXPECT_TRUE(rep1Msg[2].size() == 0);
    EXPECT_EQ(body2, dsb::comm::ToString(rep1Msg[3]));
    // Send reply from server 1 to client 2
    rep1Msg[3] = dsb::comm::ToFrame(body3);
    dsb::comm::Send(rep1, rep1Msg);

    // Server 2: Receive request from client 1
    std::deque<zmq::message_t> rep2Msg;
    dsb::comm::Receive(rep2, rep2Msg);
    ASSERT_EQ(4u, rep2Msg.size());
    EXPECT_TRUE(rep2Msg[0].size() == 0);
    EXPECT_EQ(client1Id, dsb::comm::ToString(rep2Msg[1]));
    EXPECT_TRUE(rep2Msg[2].size() == 0);
    EXPECT_EQ(body1, dsb::comm::ToString(rep2Msg[3]));
    // Send reply from server 1 to client 2
    rep2Msg[3] = dsb::comm::ToFrame(body4);
    dsb::comm::Send(rep2, rep2Msg);

    // Client 1: Receive reply from server 2
    std::deque<zmq::message_t> recvRep2Msg;
    dsb::comm::Receive(req1, recvRep2Msg);
    ASSERT_EQ(3u, recvRep2Msg.size());
    EXPECT_EQ(server2Id, dsb::comm::ToString(recvRep2Msg[0]));
    EXPECT_TRUE(recvRep2Msg[1].size() == 0);
    EXPECT_EQ(body4, dsb::comm::ToString(recvRep2Msg[2]));

    // Client 2: Receive reply from server 1
    std::deque<zmq::message_t> recvRep1Msg;
    dsb::comm::Receive(req2, recvRep1Msg);
    ASSERT_EQ(3u, recvRep1Msg.size());
    EXPECT_EQ(server1Id, dsb::comm::ToString(recvRep1Msg[0]));
    EXPECT_TRUE(recvRep1Msg[1].size() == 0);
    EXPECT_EQ(body3, dsb::comm::ToString(recvRep1Msg[2]));

    proxy.send("", 0);
}
