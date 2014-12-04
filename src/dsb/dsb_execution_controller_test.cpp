#include "gtest/gtest.h"
#include "dsb/execution/controller.hpp"

using namespace dsb::execution;

TEST(dsb_execution, Controller)
{
    const auto execLoc =
        dsb::execution::Locator("tcp://localhost:59999", "", "", "", "", "");
    auto exec = dsb::execution::Controller(execLoc);

    // Unfortunately, at the current time, we can only check whether the
    // functions correctly handle the most trivial errors, and not whether they
    // actually do what they're supposed to do.  The latter requires us to have
    // actual slaves present, which is hard to achieve in simple unittests.

    EXPECT_NO_THROW(exec.SetSimulationTime(1.0));
    EXPECT_NO_THROW(exec.SetSimulationTime(1.0, 2.0));
    EXPECT_THROW(exec.SetSimulationTime(2.0, 1.0), std::logic_error);
    EXPECT_NO_THROW(exec.SetSimulationTime(1.0, 2.0));

    EXPECT_NO_THROW(exec.AddSlave(1));
    EXPECT_NO_THROW(exec.AddSlave(2));
    EXPECT_THROW(exec.AddSlave(1), std::logic_error);
    EXPECT_NO_THROW(exec.AddSlave(3));
    EXPECT_THROW(exec.SetSimulationTime(1.0, 2.0), std::logic_error);

    EXPECT_NO_THROW(exec.SetVariables(1, dsb::sequence::EmptySequence<dsb::model::VariableValue>()));
    dsb::model::VariableValue v[2] = { { 1, 3.14 }, { 2, -10.0 } };
    EXPECT_NO_THROW(exec.SetVariables(2, dsb::sequence::ElementsOf(v, 2)));
    EXPECT_THROW(exec.SetVariables(4, dsb::sequence::EmptySequence<dsb::model::VariableValue>()), std::logic_error);
    EXPECT_THROW(exec.SetVariables(5, dsb::sequence::ElementsOf(v, 2)), std::logic_error);

    EXPECT_NO_THROW(exec.ConnectVariables(1, dsb::sequence::EmptySequence<dsb::model::VariableConnection>()));
    dsb::model::VariableConnection c[3] = { { 0, 2, 0 }, { 0, 3, 0 }, { 0, 4, 0 } };
    EXPECT_NO_THROW(exec.ConnectVariables(2, dsb::sequence::ElementsOf(c, 2)));
    EXPECT_THROW(exec.ConnectVariables(4, dsb::sequence::EmptySequence<dsb::model::VariableConnection>()), std::logic_error);
    EXPECT_THROW(exec.ConnectVariables(5, dsb::sequence::ElementsOf(c, 2)), std::logic_error);
    // The third connection has an invalid slave ID (4).
    EXPECT_THROW(exec.ConnectVariables(1, dsb::sequence::ElementsOf(c, 3)), std::logic_error);
    
    EXPECT_NO_THROW(exec.Terminate());
}
