#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <coral/fmi/importer.hpp>
#include <coral/fmi/fmu2.hpp>
#include <coral/util.hpp>


#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
namespace
{
    const std::string fmuDir = STRINGIFY(CORAL_TEST_FMU_DIRECTORY);
}


TEST(coral_fmi, Fmu2)
{
    auto importer = coral::fmi::Importer::Create();
    const std::string modelName = "WaterTank_Control";
    auto fmu = importer->Import(
        boost::filesystem::path(fmuDir) / "fmi2_cs" / (modelName+".fmu"));

    EXPECT_EQ(coral::fmi::FMIVersion::v2_0, fmu->FMIVersion());
    const auto& d = fmu->Description();
    EXPECT_EQ("WaterTank.Control", d.Name());
    EXPECT_EQ("{ad6d7bad-97d1-4fb9-ab3e-00a0d051e42c}", d.UUID());
    EXPECT_TRUE(d.Description().empty());
    EXPECT_TRUE(d.Author().empty());
    EXPECT_TRUE(d.Version().empty());
    EXPECT_NE(nullptr, std::static_pointer_cast<coral::fmi::FMU2>(fmu)->FmilibHandle());

    auto instance = fmu->InstantiateSlave();
    instance->Setup("testSlave", "testExecution", 0.0, 1.0, false, 0.0);

    bool foundValve = false;
    bool foundMinlevel = false;
    for (const auto& v : d.Variables()) {
        if (v.Name() == "valve") {
            foundValve = true;
            EXPECT_EQ(coral::model::CONTINUOUS_VARIABILITY, v.Variability());
            EXPECT_EQ(coral::model::OUTPUT_CAUSALITY, v.Causality());
            EXPECT_EQ(0.0, instance->GetRealVariable(v.ID()));
        } else if (v.Name() == "minlevel") {
            foundMinlevel = true;
            EXPECT_EQ(coral::model::FIXED_VARIABILITY, v.Variability());
            EXPECT_EQ(coral::model::PARAMETER_CAUSALITY, v.Causality());
            EXPECT_EQ(1.0, instance->GetRealVariable(v.ID()));
        }
    }
    EXPECT_TRUE(foundValve);
    EXPECT_TRUE(foundMinlevel);
}
