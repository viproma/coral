/**
\file
\brief Main header file for dsb::execution.
*/
#ifndef DSB_EXECUTION_HPP
#define DSB_EXECUTION_HPP

#include <memory>
#include <string>
#include "zmq.hpp"


namespace dsb
{

/// Functions and classes for controlling and interacting with an execution.
namespace execution
{


class Controller
{
public:
    Controller(zmq::socket_t socket);
    Controller(Controller&& other);
    Controller& operator=(Controller&& other);
    void Step(double t, double dt);
private:
    zmq::socket_t m_socket;
};


Controller SpawnController(
    std::shared_ptr<zmq::context_t> context,
    const std::string& endpoint);


}}      //namespace
#endif  // header guard