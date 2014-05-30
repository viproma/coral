#include "mock_library.hpp"

#include <vector>
#include "dsb/compat_helpers.hpp"


namespace
{
    class MockLibrary : public dsb::library::Library
    {
    public:
        MockLibrary()
        {
            using namespace dsb::library;

            // mass_1d
            {
                std::vector<VariableInfo> vars;
                vars.push_back(VariableInfo("force_x", REAL_DATATYPE, INPUT_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("pos_x", REAL_DATATYPE, OUTPUT_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("state_pos_x", REAL_DATATYPE, LOCAL_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("mass", REAL_DATATYPE, PARAMETER_CAUSALITY, TUNABLE_VARIABILITY));
                m_slaveTypes.push_back(SlaveType("mass_1d", vars.begin(), vars.end()));
            }

            // spring_1d
            {
                std::vector<VariableInfo> vars;
                vars.push_back(VariableInfo("pos_a_x", REAL_DATATYPE, INPUT_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("pos_b_x", REAL_DATATYPE, INPUT_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("force_a_x", REAL_DATATYPE, OUTPUT_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("force_b_x", REAL_DATATYPE, OUTPUT_CAUSALITY, CONTINUOUS_VARIABILITY));
                vars.push_back(VariableInfo("length", REAL_DATATYPE, PARAMETER_CAUSALITY, FIXED_VARIABILITY));
                vars.push_back(VariableInfo("stiffness", REAL_DATATYPE, PARAMETER_CAUSALITY, TUNABLE_VARIABILITY));
                m_slaveTypes.push_back(SlaveType("spring_1d", vars.begin(), vars.end()));
            }
        }

        dsb::sequence::Sequence<const dsb::library::SlaveType&> SlaveTypes()
            DSB_FINAL override
        {
            return dsb::sequence::ConstSequence(
                dsb::sequence::ContainerSequence(m_slaveTypes));
        }

        bool RequestInstantiation(
                const dsb::library::SlaveType& type,
                const std::string& name,
                const dsb::library::ExecutionLocator& execution)
            DSB_FINAL override
        {
            return false;
        }

    private:
        std::vector<dsb::library::SlaveType> m_slaveTypes;
    };


}


std::unique_ptr<dsb::library::Library> CreateMockLibrary()
{
    return std::make_unique<MockLibrary>();
}

