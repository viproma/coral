#include "boost/filesystem.hpp"
#include "gtest/gtest.h"

#include "coral/fmi/importer.hpp"
#include "coral/fmi/fmu1.hpp"
#include "coral/util.hpp"


#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
const std::string fmuDir = STRINGIFY(CORAL_TEST_FMU_DIRECTORY);


TEST(coral_fmi, Fmu1_import_fmu_cs)
{
    auto importer = coral::fmi::Importer::Create();
    const std::string modelName = "compute";
    auto fmu = importer->Import(
        boost::filesystem::path(fmuDir) / "fmi1_cs" / (modelName+".fmu"));

    ASSERT_EQ(coral::fmi::FMIVersion::v1_0, fmu->FMIVersion());
    const auto& d = fmu->Description();
    EXPECT_EQ(modelName, d.Name());
    EXPECT_EQ("2486324958", d.UUID());
    EXPECT_TRUE(d.Description().empty());
    EXPECT_TRUE(d.Author().empty());
    EXPECT_TRUE(d.Version().empty());
    EXPECT_NE(nullptr, std::static_pointer_cast<coral::fmi::FMU1>(fmu)->FmilibHandle());
}
