#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <coral/fmi/importer.hpp>
#include <coral/fmi/fmu.hpp>
#include <coral/master/execution.hpp>
#include <coral/model.hpp>
#include <coral/net.hpp>
#include <coral/slave/instance.hpp>
#include <coral/slave/runner.hpp>
#include <coral/util.hpp>


namespace
{
    class SimpleLogger : public coral::slave::Instance
    {
    public:
        SimpleLogger(std::size_t inputCount)
            : m_inputCount(inputCount)
            , m_currentValues(inputCount, 0.0)
        {
        }

        const std::map<coral::model::TimePoint, std::vector<double>>& Log() const
        {
            return m_previousValues;
        }

        // === coral::slave::Instance interface implementation ===

        coral::model::SlaveTypeDescription TypeDescription() const override
        {
            std::vector<coral::model::VariableDescription> variableDescriptions;
            for (std::size_t i = 0; i < m_inputCount; ++i) {
                variableDescriptions.emplace_back(
                    static_cast<coral::model::VariableID>(i),
                    "input[" + std::to_string(i) + "]",
                    coral::model::REAL_DATATYPE,
                    coral::model::INPUT_CAUSALITY,
                    coral::model::CONTINUOUS_VARIABILITY);
            }
            return coral::model::SlaveTypeDescription(
                "coral.test.internal.SimpleLogger",
                "4a29b80c-bd70-4d86-b1ea-fc1b48b86ebe",
                "Slave type used internally in Coral test suite",
                "Coral developers",
                "0.1",
                variableDescriptions);
        }

        void Setup(
            const std::string& /*slaveName*/,
            const std::string& /*executionName*/,
            coral::model::TimePoint /*startTime*/,
            coral::model::TimePoint /*stopTime*/,
            bool /*adaptiveStepSize*/,
            double /*relativeTolerance*/) override { }

        void StartSimulation() override
        {
            m_previousValues.clear();
        }

        void EndSimulation() override { }

        bool DoStep(
            coral::model::TimePoint currentT,
            coral::model::TimeDuration /*deltaT*/) override
        {
            m_previousValues[currentT] = m_currentValues;
            return true;
        }

        double GetRealVariable(coral::model::VariableID variable) const override
        {
            return m_currentValues[variable];
        }

        int GetIntegerVariable(coral::model::VariableID /*variable*/) const override { assert(false); return 0; }

        bool GetBooleanVariable(coral::model::VariableID /*variable*/) const override { assert(false); return false; }

        std::string GetStringVariable(coral::model::VariableID /*variable*/) const override { assert(false); return std::string(); }

        bool SetRealVariable(coral::model::VariableID variable, double value) override
        {
            m_currentValues[variable] = value;
            return true;
        }

        bool SetIntegerVariable(coral::model::VariableID /*variable*/, int /*value*/) override { assert(false); return false; }

        bool SetBooleanVariable(coral::model::VariableID /*variable*/, bool /*value*/) override { assert(false); return false; }

        bool SetStringVariable(coral::model::VariableID /*variable*/, const std::string& /*value*/) override { assert(false); return false; }

    private:
        std::size_t m_inputCount;
        std::vector<double> m_currentValues;
        std::map<coral::model::TimePoint, std::vector<double>> m_previousValues;
    };

    struct Slave
    {
        std::shared_ptr<coral::slave::Instance> instance;
        coral::net::SlaveLocator locator;
        std::thread thread;
    };

    void RunSlave(
        std::shared_ptr<coral::slave::Instance> instance,
        const coral::net::Endpoint& controlEndpoint,
        const coral::net::Endpoint& dataPubEndpoint,
        std::chrono::seconds commTimeout)
    {
        coral::slave::Runner(instance, controlEndpoint, dataPubEndpoint, commTimeout)
            .Run();
    }

    Slave SpawnSlave(std::shared_ptr<coral::slave::Instance> instance)
    {
        Slave s;
        s.instance = instance;
        s.locator = coral::net::SlaveLocator(
            coral::net::Endpoint("inproc", coral::util::RandomUUID()),
            coral::net::Endpoint("inproc", coral::util::RandomUUID()));
        s.thread = std::thread(RunSlave,
            instance,
            s.locator.ControlEndpoint(),
            s.locator.DataPubEndpoint(),
            std::chrono::seconds(10));
        return s;
    }
}


TEST(coral_master, Execution)
{
    using namespace coral::master;
    using namespace coral::model;
    const auto timeout = std::chrono::seconds(1);

    // Import 'identity' FMU
    const auto testDataDir = std::getenv("CORAL_TEST_DATA_DIR");
    auto importer = coral::fmi::Importer::Create();
    auto idFMU = importer->Import(
        boost::filesystem::path(testDataDir) / "fmi1_cs" / "identity.fmu");

    // Determine the IDs of its real-valued input and output variables
    const auto variableDescriptions = idFMU->Description().Variables();

    const auto idRealInIt = std::find_if(
        variableDescriptions.begin(),
        variableDescriptions.end(),
        [] (const VariableDescription& v) { return v.Name() == "realIn"; });
    ASSERT_FALSE(idRealInIt == variableDescriptions.end());
    const auto idRealInID = idRealInIt->ID();

    const auto idRealOutIt = std::find_if(
        variableDescriptions.begin(),
        variableDescriptions.end(),
        [] (const VariableDescription& v) { return v.Name() == "realOut"; });
    ASSERT_FALSE(idRealOutIt == variableDescriptions.end());
    const auto idRealOutID = idRealOutIt->ID();

    // Spawn slave threads
    auto idSlave1 = SpawnSlave(idFMU->InstantiateSlave());
    auto joinID1 = coral::util::OnScopeExit([&idSlave1] () { idSlave1.thread.join(); });

    auto idSlave2 = SpawnSlave(idFMU->InstantiateSlave());
    auto joinID2 = coral::util::OnScopeExit([&idSlave2] () { idSlave2.thread.join(); });

    auto logSlaveInstance = std::make_shared<SimpleLogger>(2);
    auto logSlave = SpawnSlave(logSlaveInstance);
    auto joinLog = coral::util::OnScopeExit([&logSlave] () { logSlave.thread.join(); });

    // Add slaves to execution
    auto execution = Execution("coral_test_execution");

    auto slaves = std::vector<coral::master::AddedSlave>{
        AddedSlave(idSlave1.locator, "id1"),
        AddedSlave(idSlave2.locator, "id2"),
        AddedSlave(logSlave.locator, "log")
    };
    execution.Reconstitute(slaves, timeout);

    const auto idSlave1ID = slaves[0].id;
    const auto idSlave2ID = slaves[1].id;
    const auto logSlaveID = slaves[2].id;

    // Make connections and set initial values
    auto initialSettings = std::vector<SlaveConfig>{
        SlaveConfig(
            idSlave1ID,
            std::vector<VariableSetting>{
                VariableSetting(idRealInID, 1.0)
            }),
        SlaveConfig(
            idSlave2ID,
            std::vector<VariableSetting>{
                VariableSetting(idRealInID, 2.0)
            }),
        SlaveConfig(
            logSlaveID,
            std::vector<VariableSetting>{
                VariableSetting(0, Variable(idSlave1ID, idRealOutID)),
                VariableSetting(1, Variable(idSlave2ID, idRealOutID))
            })
    };
    execution.Reconfigure(initialSettings, timeout);

    // Perform a couple of timesteps
    execution.Step(1.0, timeout);
    execution.AcceptStep(timeout);
    execution.Step(2.0, timeout);
    execution.AcceptStep(timeout);

    // Reconfigure with new inputs for id1 and id2, and switch their
    // connections to log.
    auto newSettings1 = std::vector<SlaveConfig>{
        SlaveConfig(
            idSlave1ID,
            std::vector<VariableSetting>{
                VariableSetting(idRealInID, 3.0)
            }),
        SlaveConfig(
            idSlave2ID,
            std::vector<VariableSetting>{
                VariableSetting(idRealInID, 4.0)
            }),
        SlaveConfig(
            logSlaveID,
            std::vector<VariableSetting>{
                VariableSetting(0, Variable(idSlave2ID, idRealOutID)),
                VariableSetting(1, Variable(idSlave1ID, idRealOutID))
            })
    };
    execution.Reconfigure(newSettings1, timeout);

    execution.Step(3.0, timeout);
    execution.AcceptStep(timeout);
    execution.Step(4.0, timeout);
    execution.AcceptStep(timeout);

    // Reconfigure with new inputs for id1 and id2, and *break* the
    // connection between id2 and log.
    auto newSettings2 = std::vector<SlaveConfig>{
        SlaveConfig(
            idSlave1ID,
            std::vector<VariableSetting>{
                VariableSetting(idRealInID, 5.0)
            }),
        SlaveConfig(
            idSlave2ID,
            std::vector<VariableSetting>{
                VariableSetting(idRealInID, 6.0)
            }),
        SlaveConfig(
            logSlaveID,
            std::vector<VariableSetting>{
                VariableSetting(0, Variable())
            })
    };
    execution.Reconfigure(newSettings2, timeout);

    execution.Step(5.0, timeout);
    execution.AcceptStep(timeout);
    execution.Step(6.0, timeout);
    execution.AcceptStep(timeout);

    // Check that the time points are all there.
    const auto log = logSlaveInstance->Log();
    ASSERT_EQ(6U, log.size()); // 6 time steps performed
    ASSERT_TRUE(log.count( 0.0) == 1);
    ASSERT_TRUE(log.count( 1.0) == 1);
    ASSERT_TRUE(log.count( 3.0) == 1);
    ASSERT_TRUE(log.count( 6.0) == 1);
    ASSERT_TRUE(log.count(10.0) == 1);
    ASSERT_TRUE(log.count(15.0) == 1);

    // Verify that the values of the variables are as expected.
    // (Some tests are commented out due to issue #49.)
    const auto vec2Equals = [] (const std::vector<double>& v, double e0, double e1) {
        return v.size() == 2 && v[0] == e0 && v[1] == e1;
    };
    EXPECT_TRUE(vec2Equals(log.at( 0.0), 1.0, 2.0));
    EXPECT_TRUE(vec2Equals(log.at( 1.0), 1.0, 2.0));
    //EXPECT_TRUE(vec2Equals(log.at( 3.0), 4.0, 3.0));
    EXPECT_TRUE(vec2Equals(log.at( 6.0), 4.0, 3.0));
    //EXPECT_TRUE(vec2Equals(log.at(10.0), 4.0, 5.0));
    EXPECT_TRUE(vec2Equals(log.at(15.0), 4.0, 5.0));

    execution.Terminate();
}
