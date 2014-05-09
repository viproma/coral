#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "boost/lexical_cast.hpp"
#include "zmq.hpp"

#include "dsb/util/encoding.hpp"
#include "control.pb.h"


int main(int argc, const char** argv)
{
    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <id>\n"
                  << "  id = a number in the range 0 - 65535" << std::endl;
        return 0;
    }

    const auto id = boost::lexical_cast<uint16_t>(argv[1]);
    std::clog << "Using ID " << id << std::endl;

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_REQ);
}
