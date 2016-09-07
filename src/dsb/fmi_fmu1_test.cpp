#include "boost/filesystem.hpp"
#include "gtest/gtest.h"

#include "dsb/fmi/importer.hpp"
#include "dsb/fmi/fmu1.hpp"
#include "dsb/util.hpp"


#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
const std::string fmuDir = STRINGIFY(DSB_TEST_FMU_DIRECTORY);


TEST(dsb_fmi, Fmu1_import_fmu_cs)
{
    auto importer = dsb::fmi::Importer::Create();
    const std::string modelName = "compute";
    auto fmu = importer->Import(
        boost::filesystem::path(fmuDir) / "fmi1_cs" / (modelName+".fmu"));

    ASSERT_EQ(dsb::fmi::FMIVersion::v1_0, fmu->FMIVersion());
    const auto& d = fmu->Description();
    EXPECT_EQ(modelName, d.Name());
    EXPECT_EQ("2486324958", d.UUID());
    EXPECT_TRUE(d.Description().empty());
    EXPECT_TRUE(d.Author().empty());
    EXPECT_TRUE(d.Version().empty());
    EXPECT_NE(nullptr, std::static_pointer_cast<dsb::fmi::FMU1>(fmu)->FmilibHandle());
}
